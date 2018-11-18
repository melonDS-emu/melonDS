#include "OboeCallback.h"
#include "../types.h"
#include "../SPU.h"

oboe::DataCallbackResult
OboeCallback::onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {
    // resampling:
    // buffer length is 1024 samples
    // which is 710 samples at the original sample rate

    //SPU::ReadOutput((s16*)stream, len>>2);
    s16 buf_in[710*2];
    s16* buf_out = (s16*)audioData;

    int num_in = SPU::ReadOutput(buf_in, 710);
    int num_out = 1024;

    int margin = 6;
    if (num_in < 710-margin)
    {
        int last = num_in-1;
        if (last < 0) last = 0;

        for (int i = num_in; i < 710-margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = 710-margin;
    }

    float res_incr = num_in / (float)num_out;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < 1024; i++)
    {
        // TODO: interp!!
        buf_out[i*2  ] = buf_in[res_pos*2  ];
        buf_out[i*2+1] = buf_in[res_pos*2+1];

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
    return oboe::DataCallbackResult::Continue;
}
