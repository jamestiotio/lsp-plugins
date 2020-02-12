/*
 * Oscilloscope.cpp
 *
 *  Created on: 27 Jan 2020
 *      Author: crocoduck
 */

#include <core/util/Oscilloscope.h>

#define CAPTURE_BUFFER_LIMIT_SIZE   196608 // Multiple of 3 4 6 and 8
#define SWEEP_BUFFER_LIMIT_SIZE     196608 // Multiple of 3 4 6 and 8

namespace lsp
{
    Oscilloscope::Oscilloscope()
    {
        nSampleRate                 = -1;
        nOverSampleRate             = -1;

        enTriggerType               = TRG_TYPE_NONE;

        enState                     = OSC_STATE_ACQUIRING;

        sSweepParams.fPreTrigger    = 0.0f;
        sSweepParams.fPostTrigger   = 0.0f;
        sSweepParams.nPreTrigger    = 0;
        sSweepParams.nPostTrigger   = 0;
        sSweepParams.fVertOffset    = 0.0f;
        sSweepParams.fHorOffset     = 0.0f;
        sSweepParams.nLimit         = 0;
        sSweepParams.nHead          = 0;
        sSweepParams.bSweepComplete = false;

        sBufferParams.nHead         = 0;
        sBufferParams.nTriggerAt    = 0;
//        sBufferParams.nRemaining    = CAPTURE_BUFFER_LIMIT_SIZE;

        enOverMode                  = OM_NONE;
        nOversampling               = 0;

        enOutputMode                = OSC_OUTPUT_MODE_MUTED;

        vCaptureBuffer              = NULL;
        vSweepBuffer                 = NULL;
        pData                       = NULL;

        bSync                       = true;
    }

    Oscilloscope::~Oscilloscope()
    {
    }

    bool Oscilloscope::init()
    {
        // 1X Capture Buffer + 1X Sync Buffer
        size_t samples = CAPTURE_BUFFER_LIMIT_SIZE + SWEEP_BUFFER_LIMIT_SIZE;

        float *ptr = alloc_aligned<float>(pData, samples);
        if (ptr == NULL)
            return false;

        lsp_guard_assert(float *save = ptr);

        vCaptureBuffer = ptr;
        ptr += CAPTURE_BUFFER_LIMIT_SIZE;

        vSweepBuffer = ptr;
        ptr += SWEEP_BUFFER_LIMIT_SIZE;

        lsp_assert(ptr <= &save[samples]);

        return sOver.init() && sTrigger.init();
    }

    void Oscilloscope::destroy()
    {
        free_aligned(pData);
        pData = NULL;
        vCaptureBuffer = NULL;
        vSweepBuffer = NULL;

        sOver.destroy();
    }

    void Oscilloscope::update_settings()
    {
        if (!bSync)
            return;

        sOver.set_sample_rate(nSampleRate);
        sOver.set_mode(enOverMode);
        if (sOver.modified())
            sOver.update_settings();
        nOversampling = sOver.get_oversampling();
        nOverSampleRate = nOversampling * nSampleRate;

        size_t minBufSize = (CAPTURE_BUFFER_LIMIT_SIZE < SWEEP_BUFFER_LIMIT_SIZE) ? CAPTURE_BUFFER_LIMIT_SIZE : SWEEP_BUFFER_LIMIT_SIZE;

        sSweepParams.nPreTrigger = seconds_to_samples(nOverSampleRate, sSweepParams.fPreTrigger);
        sSweepParams.nPreTrigger = (sSweepParams.nPreTrigger < minBufSize) ? sSweepParams.nPreTrigger : minBufSize;
        sSweepParams.fPreTrigger = samples_to_seconds(nOverSampleRate, sSweepParams.nPreTrigger);

        size_t availableForPost = SWEEP_BUFFER_LIMIT_SIZE - sSweepParams.nPreTrigger;
        sSweepParams.nPostTrigger = seconds_to_samples(nOverSampleRate, sSweepParams.fPostTrigger);
        sSweepParams.nPostTrigger = (sSweepParams.nPostTrigger < availableForPost) ? sSweepParams.nPostTrigger : availableForPost;
        sSweepParams.fPostTrigger = samples_to_seconds(nOverSampleRate, sSweepParams.nPostTrigger);

        sSweepParams.nLimit = sSweepParams.nPreTrigger + sSweepParams.nPostTrigger;
        sSweepParams.nHead = 0;
        sSweepParams.bSweepComplete = false;

        sBufferParams.nHead = 0;
        sBufferParams.nTriggerAt = 0;
//        sBufferParams.nRemaining = CAPTURE_BUFFER_LIMIT_SIZE;

        sTrigger.set_post_trigger_samples(sSweepParams.nPostTrigger);
        sTrigger.set_trigger_type(enTriggerType);
        sTrigger.update_settings();

        bSync = false;
    }

    // Copying the samples from the past (with respect to the trigger) into the sweep buffer. Excluding trigger sample.
    void Oscilloscope::sweep_from_the_past()
    {
        size_t copyHead = (sBufferParams.nTriggerAt < sSweepParams.nPreTrigger) ? CAPTURE_BUFFER_LIMIT_SIZE - sSweepParams.nPreTrigger + sBufferParams.nTriggerAt : sBufferParams.nTriggerAt - sSweepParams.nPreTrigger;

       if (copyHead >= sBufferParams.nTriggerAt)
       {
           dsp::copy(&vSweepBuffer[sSweepParams.nHead], &vCaptureBuffer[copyHead], CAPTURE_BUFFER_LIMIT_SIZE - copyHead);
           sSweepParams.nHead += CAPTURE_BUFFER_LIMIT_SIZE - copyHead;

           dsp::copy(&vSweepBuffer[sSweepParams.nHead], vCaptureBuffer, sBufferParams.nTriggerAt);
           sSweepParams.nHead += sBufferParams.nTriggerAt;
       }
       else
       {
           dsp::copy(&vSweepBuffer[sSweepParams.nHead], &vCaptureBuffer[copyHead], sBufferParams.nTriggerAt - copyHead);
           sSweepParams.nHead += sBufferParams.nTriggerAt - copyHead;
       }
    }

    void Oscilloscope::process(float *dst, float *src, size_t count)
    {
        switch (enOutputMode)
        {
            case OSC_OUTPUT_MODE_COPY:
                dsp::copy(dst, src, count);
                break;
            case OSC_OUTPUT_MODE_MUTED:
            default:
                dsp::fill_zero(dst, count);
                break;
        }

        while (count > 0)
        {
            switch (enState)
            {
                case OSC_STATE_ACQUIRING:
                {
                    size_t to_process = nOversampling * count;
                    size_t remaining = CAPTURE_BUFFER_LIMIT_SIZE - sBufferParams.nHead;
                    size_t to_store = to_process > remaining ? remaining : to_process;
                    size_t to_do = to_store / nOversampling;

                    float* bufptr = &vCaptureBuffer[sBufferParams.nHead];

                    sOver.upsample(bufptr, src, to_do);

                    for (size_t n = 0; n < to_store; ++n)
                    {
                        sTrigger.single_sample_processor(bufptr[n]);

                        trg_state_t tState = sTrigger.get_trigger_state();

                        if (tState == TRG_STATE_FIRED)
                        {
                            enState = OSC_STATE_SWEEPING;
                            sBufferParams.nTriggerAt = sBufferParams.nHead + n;
                            sSweepParams.nHead = 0;
                            sSweepParams.bSweepComplete = false;
                            sweep_from_the_past();
                        }
                    }

                    sBufferParams.nHead = (sBufferParams.nHead + to_store) % CAPTURE_BUFFER_LIMIT_SIZE;

                    count -= to_do;

                    break;
                }
                case OSC_STATE_SWEEPING:
                {
                    size_t copyTail = (sBufferParams.nTriggerAt + sSweepParams.nPostTrigger) % CAPTURE_BUFFER_LIMIT_SIZE;
                    copyTail = (copyTail < sBufferParams.nHead) ? copyTail : sBufferParams.nHead;

                    if (sBufferParams.nTriggerAt <= copyTail)
                    {
                        dsp::copy(&vSweepBuffer[sSweepParams.nHead], &vCaptureBuffer[sBufferParams.nTriggerAt], copyTail - sBufferParams.nTriggerAt + 1);
                        sSweepParams.nHead += copyTail - sBufferParams.nTriggerAt + 1;
                    }
                    else
                    {
                        dsp::copy(&vSweepBuffer[sSweepParams.nHead], &vCaptureBuffer[sBufferParams.nTriggerAt], CAPTURE_BUFFER_LIMIT_SIZE - sBufferParams.nTriggerAt);
                        sBufferParams.nTriggerAt += CAPTURE_BUFFER_LIMIT_SIZE - sBufferParams.nTriggerAt;

                        dsp::copy(&vSweepBuffer[sSweepParams.nHead], vCaptureBuffer, copyTail + 1);
                        sSweepParams.nHead += copyTail + 1;
                    }

                    size_t to_process = nOversampling * count;
                    size_t remaining = CAPTURE_BUFFER_LIMIT_SIZE - sBufferParams.nHead;
                    size_t to_store = to_process > remaining ? remaining : to_process;
                    size_t to_do = to_store / nOversampling;

                    sOver.upsample(&vCaptureBuffer[sBufferParams.nHead], src, to_do);

                    sBufferParams.nHead = (sBufferParams.nHead + to_store) % CAPTURE_BUFFER_LIMIT_SIZE;

                    count -= to_do;

                    if (sSweepParams.nHead >= sSweepParams.nLimit - 1)
                    {
                        enState = OSC_STATE_ACQUIRING;
                        sSweepParams.nHead = 0;
                        sSweepParams.bSweepComplete = true;
                    }

                    break;
                }
            }
        }
    }
}