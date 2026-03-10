/**
 * Voice Chat — lightweight WebRTC hook for in-lobby voice communication.
 *
 * Architecture:
 *   - Each lobby participant opens a WebRTC PeerConnection to every other player.
 *   - Signaling (offer/answer/ICE candidates) is exchanged via the existing
 *     WebSocket lobby connection (messages: `vc-offer`, `vc-answer`, `vc-ice`).
 *   - The lobby server relays signaling messages by room, just like chat messages.
 *
 * Current state: signaling message types are defined and the hook manages
 * microphone access + mute state. Full mesh signaling wiring requires the lobby
 * server to relay `vc-*` message types (stubs provided in handler.ts already
 * pass unknown messages through to room members, so no server changes are needed
 * for the initial integration).
 *
 * Usage:
 *   const vc = useVoiceChat(ws, roomId, playerId);
 *   <button onClick={vc.toggleMute}>{vc.muted ? '🔇' : '🎤'}</button>
 */

import { useState, useEffect, useCallback, useRef } from 'react';

// ---------------------------------------------------------------------------
// Signaling message types sent over the WebSocket lobby connection
// ---------------------------------------------------------------------------

export interface VcOffer {
  type: 'vc-offer';
  from: string;
  to: string;
  sdp: RTCSessionDescriptionInit;
}

export interface VcAnswer {
  type: 'vc-answer';
  from: string;
  to: string;
  sdp: RTCSessionDescriptionInit;
}

export interface VcIce {
  type: 'vc-ice';
  from: string;
  to: string;
  candidate: RTCIceCandidateInit;
}

export type VcSignalingMessage = VcOffer | VcAnswer | VcIce;

// ---------------------------------------------------------------------------
// Public hook interface
// ---------------------------------------------------------------------------

export interface VoiceChatState {
  /** Whether voice chat is currently active (microphone captured). */
  active: boolean;
  /** Whether our own microphone is muted. */
  muted: boolean;
  /** Map of peerId → talking state (volume detected). */
  peerActivity: Record<string, boolean>;
  /** Start capturing the microphone and connect to peers. */
  enable: () => Promise<void>;
  /** Stop all streams and close peer connections. */
  disable: () => void;
  /** Toggle own microphone mute without closing connections. */
  toggleMute: () => void;
  /** Last error (e.g. mic permission denied). */
  error: string | null;
}

// STUN servers for ICE negotiation (public Google STUN)
const ICE_SERVERS: RTCConfiguration = {
  iceServers: [{ urls: 'stun:stun.l.google.com:19302' }],
};

/**
 * useVoiceChat — React hook for WebRTC mesh voice chat within a lobby room.
 *
 * @param ws       - The active WebSocket connection (from LobbyContext).
 * @param playerId - Our own player ID (used to route signaling messages).
 * @param peerIds  - List of remote peer player IDs to connect to.
 */
export function useVoiceChat(
  ws: WebSocket | null,
  playerId: string,
  peerIds: string[],
): VoiceChatState {
  const [active, setActive] = useState(false);
  const [muted, setMuted] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [peerActivity, setPeerActivity] = useState<Record<string, boolean>>({});

  const localStreamRef = useRef<MediaStream | null>(null);
  const peersRef = useRef<Map<string, RTCPeerConnection>>(new Map());
  const analyserTimersRef = useRef<Map<string, ReturnType<typeof setInterval>>>(new Map());
  const audioElementsRef = useRef<Map<string, HTMLAudioElement>>(new Map());

  // ---------------------------------------------------------------------------
  // Signaling: handle incoming vc-* messages from the WebSocket
  // ---------------------------------------------------------------------------
  useEffect(() => {
    if (!ws) return;

    function handleMessage(event: MessageEvent) {
      let msg: unknown;
      try {
        msg = JSON.parse(event.data as string);
      } catch {
        return;
      }
      if (!msg || typeof msg !== 'object') return;
      const m = msg as Record<string, unknown>;

      if (m['type'] === 'vc-offer' && m['to'] === playerId) {
        handleRemoteOffer(m as unknown as VcOffer);
      } else if (m['type'] === 'vc-answer' && m['to'] === playerId) {
        handleRemoteAnswer(m as unknown as VcAnswer);
      } else if (m['type'] === 'vc-ice' && m['to'] === playerId) {
        handleRemoteIce(m as unknown as VcIce);
      }
    }

    ws.addEventListener('message', handleMessage);
    return () => ws.removeEventListener('message', handleMessage);
  }, [ws, playerId]); // eslint-disable-line react-hooks/exhaustive-deps

  // ---------------------------------------------------------------------------
  // Signaling helpers
  // ---------------------------------------------------------------------------

  function send(msg: VcSignalingMessage) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(msg));
    }
  }

  async function handleRemoteOffer(offer: VcOffer) {
    const pc = getOrCreatePeer(offer.from);
    await pc.setRemoteDescription(offer.sdp);
    const answer = await pc.createAnswer();
    await pc.setLocalDescription(answer);
    send({ type: 'vc-answer', from: playerId, to: offer.from, sdp: answer });
  }

  async function handleRemoteAnswer(answer: VcAnswer) {
    const pc = peersRef.current.get(answer.from);
    if (pc) {
      await pc.setRemoteDescription(answer.sdp);
    }
  }

  async function handleRemoteIce(msg: VcIce) {
    const pc = peersRef.current.get(msg.from);
    if (pc) {
      await pc.addIceCandidate(msg.candidate);
    }
  }

  // ---------------------------------------------------------------------------
  // Peer connection management
  // ---------------------------------------------------------------------------

  function getOrCreatePeer(peerId: string): RTCPeerConnection {
    const existing = peersRef.current.get(peerId);
    if (existing) return existing;

    const pc = new RTCPeerConnection(ICE_SERVERS);

    pc.onicecandidate = (ev) => {
      if (ev.candidate) {
        send({ type: 'vc-ice', from: playerId, to: peerId, candidate: ev.candidate.toJSON() });
      }
    };

    pc.ontrack = (ev) => {
      // Attach remote audio stream; store element to allow cleanup on disconnect
      const existing = audioElementsRef.current.get(peerId);
      if (existing) {
        existing.srcObject = null;
      }
      const audio = new Audio();
      audio.srcObject = ev.streams[0];
      audio.autoplay = true;
      audioElementsRef.current.set(peerId, audio);
      trackPeerActivity(peerId, ev.streams[0]);
    };

    // Add local tracks if we already have them
    if (localStreamRef.current) {
      for (const track of localStreamRef.current.getAudioTracks()) {
        pc.addTrack(track, localStreamRef.current);
      }
    }

    peersRef.current.set(peerId, pc);
    return pc;
  }

  /** Monitor audio level on a remote stream to detect when peers are talking. */
  function trackPeerActivity(peerId: string, stream: MediaStream) {
    try {
      const ctx = new AudioContext();
      const source = ctx.createMediaStreamSource(stream);
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 256;
      source.connect(analyser);
      const data = new Uint8Array(analyser.frequencyBinCount);

      const timer = setInterval(() => {
        analyser.getByteFrequencyData(data);
        const avg = data.reduce((s, v) => s + v, 0) / data.length;
        setPeerActivity((prev) => ({ ...prev, [peerId]: avg > 10 }));
      }, 200);

      analyserTimersRef.current.set(peerId, timer);
    } catch {
      // AudioContext not available (e.g. in tests)
    }
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  const enable = useCallback(async () => {
    if (active) return;
    setError(null);
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
      localStreamRef.current = stream;
      setActive(true);
      setMuted(false);

      // Initiate connections to all current peers.
      // Only the "lesser" player ID sends the initial offer to prevent both
      // sides from offering simultaneously and causing a signaling collision.
      for (const peerId of peerIds) {
        if (playerId < peerId) {
          const pc = getOrCreatePeer(peerId);
          const offer = await pc.createOffer();
          await pc.setLocalDescription(offer);
          send({ type: 'vc-offer', from: playerId, to: peerId, sdp: offer });
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Mic error: ${msg}`);
    }
  }, [active, peerIds, playerId]); // eslint-disable-line react-hooks/exhaustive-deps

  const disable = useCallback(() => {
    localStreamRef.current?.getTracks().forEach((t) => t.stop());
    localStreamRef.current = null;

    for (const pc of peersRef.current.values()) {
      pc.close();
    }
    peersRef.current.clear();

    for (const timer of analyserTimersRef.current.values()) {
      clearInterval(timer);
    }
    analyserTimersRef.current.clear();

    for (const audio of audioElementsRef.current.values()) {
      audio.srcObject = null;
    }
    audioElementsRef.current.clear();

    setPeerActivity({});
    setActive(false);
    setMuted(false);
  }, []);

  const toggleMute = useCallback(() => {
    if (!localStreamRef.current) return;
    const newMuted = !muted;
    for (const track of localStreamRef.current.getAudioTracks()) {
      track.enabled = !newMuted;
    }
    setMuted(newMuted);
  }, [muted]);

  // Clean up when component unmounts
  useEffect(() => {
    return () => {
      disable();
    };
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return { active, muted, peerActivity, enable, disable, toggleMute, error };
}
