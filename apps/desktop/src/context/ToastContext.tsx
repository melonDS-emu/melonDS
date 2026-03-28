/**
 * Minimal in-app toast notification system.
 *
 * Usage:
 *   const { addToast } = useToast();
 *   addToast({ message: 'Achievement unlocked!', icon: '🏆' });
 *
 * Mount <ToastContainer /> once (in Layout or App root) to render toasts.
 */

import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
} from 'react';
import type { ReactNode } from 'react';

export interface Toast {
  id: string;
  message: string;
  detail?: string;
  icon?: string;
  /** Duration in milliseconds before auto-dismiss. Default: 4000. */
  duration?: number;
}

interface ToastContextValue {
  toasts: Toast[];
  addToast: (toast: Omit<Toast, 'id'>) => void;
  removeToast: (id: string) => void;
}

const ToastContext = createContext<ToastContextValue | null>(null);

let _toastIdCounter = 0;

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);
  /** Track pending auto-dismiss timers so they can be cleared on unmount. */
  const timersRef = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());

  // Clear all pending timers when the provider unmounts.
  useEffect(() => {
    const timers = timersRef.current;
    return () => {
      for (const timer of timers.values()) clearTimeout(timer);
      timers.clear();
    };
  }, []);

  const removeToast = useCallback((id: string) => {
    const timer = timersRef.current.get(id);
    if (timer !== undefined) {
      clearTimeout(timer);
      timersRef.current.delete(id);
    }
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  const addToast = useCallback(
    (toast: Omit<Toast, 'id'>) => {
      const id = String(++_toastIdCounter);
      const duration = toast.duration ?? 4000;
      setToasts((prev) => [...prev, { ...toast, id }]);
      const timer = setTimeout(() => {
        timersRef.current.delete(id);
        setToasts((prev) => prev.filter((t) => t.id !== id));
      }, duration);
      timersRef.current.set(id, timer);
    },
    []
  );

  return (
    <ToastContext.Provider value={{ toasts, addToast, removeToast }}>
      {children}
    </ToastContext.Provider>
  );
}

export function useToast(): ToastContextValue {
  const ctx = useContext(ToastContext);
  if (!ctx) throw new Error('useToast must be used within a ToastProvider');
  return ctx;
}
