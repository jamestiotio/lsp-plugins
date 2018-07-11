/*
 * filters.h
 *
 *  Created on: 22 авг. 2016 г.
 *      Author: sadko
 */

#ifndef CORE_NATIVE_FILTERS_H_
#define CORE_NATIVE_FILTERS_H_

#ifndef __DSP_NATIVE_IMPL
    #error "This header should not be included directly"
#endif /* __DSP_NATIVE_IMPL */

namespace lsp
{
    namespace native
    {
        void biquad_process_x1(float *dst, const float *src, size_t count, biquad_t *f)
        {
            for (size_t i=0; i<count; ++i)
            {
                float s     = src[i];
                //   a: a0 a0 a1 a2
                //   b: b1 b2 0  0
                float s2    = f->x1.a[0]*s + f->d[0];
                float p1    = f->x1.a[2]*s + f->x1.b[0]*s2;
                float p2    = f->x1.a[3]*s + f->x1.b[1]*s2;

                dst[i]      = s2;

                // Shift buffer
                f->d[0]     = f->d[1] + p1;
                f->d[1]     = p2;
            }
        }

        void biquad_process_x2(float *dst, const float *src, size_t count, biquad_t *f)
        {
            if (count <= 0)
                return;

            float s, r, s2, r2, p1, q1, p2, q2;

            //   a: a0 a0 a1 a2 i0 i0 i1 i2
            //   b: b1 b2 0  0  j1 j2 0  0

            // First filter only
            s           = *(src++);
            s2          = f->x2.a[0]*s + f->d[0];
            p1          = f->x2.a[2]*s + f->x2.b[0]*s2;
            p2          = f->x2.a[3]*s + f->x2.b[1]*s2;
            r           = s2;
            f->d[0]     = f->d[1] + p1;
            f->d[1]     = p2;

            // Both filters
            for (size_t i=1; i<count; ++i)
            {
                s           = *(src++);
                r2          = f->x2.a[4]*r + f->d[4];
                s2          = f->x2.a[0]*s + f->d[0];

                q1          = f->x2.a[6]*r + f->x2.b[4]*r2;
                p1          = f->x2.a[2]*s + f->x2.b[0]*s2;
                q2          = f->x2.a[7]*r + f->x2.b[5]*r2;
                p2          = f->x2.a[3]*s + f->x2.b[1]*s2;

                r           = s2;
                *(dst++)    = r2;

                // Shift buffers
                f->d[4]     = f->d[5] + q1;
                f->d[0]     = f->d[1] + p1;
                f->d[5]     = q2;
                f->d[1]     = p2;
            }

            // Second filter only
            r2          = f->x2.a[4]*r + f->d[4];
            q1          = f->x2.a[6]*r + f->x2.b[4]*r2;
            q2          = f->x2.a[7]*r + f->x2.b[5]*r2;
            *dst        = r2;
            f->d[4]     = f->d[5] + q1;
            f->d[5]     = q2;
        }

        void biquad_process_x4(float *dst, const float *src, size_t count, biquad_t *f)
        {
            if (count <= 0)
                return;

            float s[4], s2[4], p1[4], p2[4];
            s[0]        = 0.0f;
            s[1]        = 0.0f;
            s[2]        = 0.0f;
            s[3]        = 0.0f;
            s2[0]       = 0.0f;
            s2[1]       = 0.0f;
            s2[2]       = 0.0f;
            s2[3]       = 0.0f;

            size_t mask = 0;
            size_t i    = 0;

            // Start filters, mask enables the specific filter
            do
            {
                // Push sample
                s[0]        = *(src++);
                mask       |= 1;

                // Calculate filters by mask and shift buffers
                s2[0]       = f->x4.a0[0]*s[0] + f->d[0];
                p1[0]       = f->x4.a1[0]*s[0] + f->x4.b1[0]*s2[0];
                p2[0]       = f->x4.a2[0]*s[0] + f->x4.b2[0]*s2[0];
                f->d[0]     = f->d[4] + p1[0];
                f->d[4]     = p2[0];

                if (mask & 0x2)
                {
                    s2[1]       = f->x4.a0[1]*s[1] + f->d[1];
                    p1[1]       = f->x4.a1[1]*s[1] + f->x4.b1[1]*s2[1];
                    p2[1]       = f->x4.a2[1]*s[1] + f->x4.b2[1]*s2[1];
                    f->d[1]     = f->d[5] + p1[1];
                    f->d[5]     = p2[1];
                }
                if (mask & 0x4)
                {
                    s2[2]       = f->x4.a0[2]*s[2] + f->d[2];
                    p1[2]       = f->x4.a1[2]*s[2] + f->x4.b1[2]*s2[2];
                    p2[2]       = f->x4.a2[2]*s[2] + f->x4.b2[2]*s2[2];
                    f->d[2]     = f->d[6] + p1[2];
                    f->d[6]     = p2[2];
                }

                // Shift buffer
                s[3]        = s2[2];
                s[2]        = s2[1];
                s[1]        = s2[0];

                // Update mask
                mask      <<= 1;
                if ((++i) >= count)
                    break;
            } while (i < 3);

            // Process all filters simultaneously
            while (i < count)
            {
                // Push sample
                s[0]        = *(src++);

                // Calculate filters by mask and shift buffers
                s2[0]       = f->x4.a0[0]*s[0] + f->d[0];
                s2[1]       = f->x4.a0[1]*s[1] + f->d[1];
                s2[2]       = f->x4.a0[2]*s[2] + f->d[2];
                s2[3]       = f->x4.a0[3]*s[3] + f->d[3];

                p1[0]       = f->x4.a1[0]*s[0] + f->x4.b1[0]*s2[0];
                p1[1]       = f->x4.a1[1]*s[1] + f->x4.b1[1]*s2[1];
                p1[2]       = f->x4.a1[2]*s[2] + f->x4.b1[2]*s2[2];
                p1[3]       = f->x4.a1[3]*s[3] + f->x4.b1[3]*s2[3];

                p2[0]       = f->x4.a2[0]*s[0] + f->x4.b2[0]*s2[0];
                p2[1]       = f->x4.a2[1]*s[1] + f->x4.b2[1]*s2[1];
                p2[2]       = f->x4.a2[2]*s[2] + f->x4.b2[2]*s2[2];
                p2[3]       = f->x4.a2[3]*s[3] + f->x4.b2[3]*s2[3];

                f->d[0]     = f->d[4] + p1[0];
                f->d[1]     = f->d[5] + p1[1];
                f->d[2]     = f->d[6] + p1[2];
                f->d[3]     = f->d[7] + p1[3];

                f->d[4]     = p2[0];
                f->d[5]     = p2[1];
                f->d[6]     = p2[2];
                f->d[7]     = p2[3];

                // Shift buffer
                *(dst++)    = s2[3];
                s[3]        = s2[2];
                s[2]        = s2[1];
                s[1]        = s2[0];

                i          ++;
            }

            // Finish processing
            do
            {
                // Calculate filters by mask and shift buffers
                if (mask & 0x2)
                {
                    s2[1]       = f->x4.a0[1]*s[1] + f->d[1];
                    p1[1]       = f->x4.a1[1]*s[1] + f->x4.b1[1]*s2[1];
                    p2[1]       = f->x4.a2[1]*s[1] + f->x4.b2[1]*s2[1];
                    f->d[1]     = f->d[5] + p1[1];
                    f->d[5]     = p2[1];
                }
                if (mask & 0x4)
                {
                    s2[2]       = f->x4.a0[2]*s[2] + f->d[2];
                    p1[2]       = f->x4.a1[2]*s[2] + f->x4.b1[2]*s2[2];
                    p2[2]       = f->x4.a2[2]*s[2] + f->x4.b2[2]*s2[2];
                    f->d[2]     = f->d[6] + p1[2];
                    f->d[6]     = p2[2];
                }

                s2[3]       = f->x4.a0[3]*s[3] + f->d[3];
                p1[3]       = f->x4.a1[3]*s[3] + f->x4.b1[3]*s2[3];
                p2[3]       = f->x4.a2[3]*s[3] + f->x4.b2[3]*s2[3];
                f->d[3]     = f->d[7] + p1[3];
                f->d[7]     = p2[3];

                // Shift buffer
                *(dst++)    = s2[3];
                s[3]        = s2[2];
                s[2]        = s2[1];
                s[1]        = s2[0];

                // Update mask
                mask        = (mask << 1) & 0x0f;
            } while (mask != 0);
        }

        void biquad_process_x8(float *dst, const float *src, size_t count, biquad_t *f)
        {
            // This code already works badly instead of biquad_process_x4
            if (count <= 0)
                return;

            float s[4], s2[4], p1[4], p2[4];
            s[0]            = 0.0f;
            s[1]            = 0.0f;
            s[2]            = 0.0f;
            s[3]            = 0.0f;
            s2[0]           = 0.0f;
            s2[1]           = 0.0f;
            s2[2]           = 0.0f;
            s2[3]           = 0.0f;

            // Calculate as two passes of x4 filters
            for (size_t n=0; n<2; ++n)
            {
                // two x4 filters are in parallel, shift by 4 floats stride
                biquad_x8_t *bq = reinterpret_cast<biquad_x8_t *>(&f->d[n*4]);
                size_t mask     = 0;
                size_t i        = 0;
                const float *sp = src;
                float *dp       = dst;

                // Start filters, mask enables the specific filter
                do
                {
                    // Push sample
                    s[0]        = *(sp++);
                    mask       |= 1;

                    // Calculate filters by mask and shift buffers
                    s2[0]       = bq->a0[0]*s[0] + f->d[0];
                    p1[0]       = bq->a1[0]*s[0] + bq->b1[0]*s2[0];
                    p2[0]       = bq->a2[0]*s[0] + bq->b2[0]*s2[0];
                    f->d[0]     = f->d[8]   + p1[0];
                    f->d[8]     = p2[0];

                    if (mask & 0x2)
                    {
                        s2[1]       = bq->a0[1]*s[1] + f->d[1];
                        p1[1]       = bq->a1[1]*s[1] + bq->b1[1]*s2[1];
                        p2[1]       = bq->a2[1]*s[1] + bq->b2[1]*s2[1];
                        f->d[1]     = f->d[9]   + p1[1];
                        f->d[9]     = p2[1];
                    }
                    if (mask & 0x4)
                    {
                        s2[2]       = bq->a0[2]*s[2] + f->d[2];
                        p1[2]       = bq->a1[2]*s[2] + bq->b1[2]*s2[2];
                        p2[2]       = bq->a2[2]*s[2] + bq->b2[2]*s2[2];
                        f->d[2]     = f->d[10]  + p1[2];
                        f->d[10]    = p2[2];
                    }

                    // Shift buffer
                    s[3]        = s2[2];
                    s[2]        = s2[1];
                    s[1]        = s2[0];

                    // Update mask
                    mask      <<= 1;
                    if ((++i) >= count)
                        break;
                } while (i < 3);

                // Process all filters simultaneously
                while (i < count)
                {
                    // Push sample
                    s[0]        = *(sp++);

                    // Calculate filters by mask and shift buffers
                    s2[0]       = bq->a0[0]*s[0] + f->d[0];
                    s2[1]       = bq->a0[1]*s[1] + f->d[1];
                    s2[2]       = bq->a0[2]*s[2] + f->d[2];
                    s2[3]       = bq->a0[3]*s[3] + f->d[3];

                    p1[0]       = bq->a1[0]*s[0] + bq->b1[0]*s2[0];
                    p1[1]       = bq->a1[1]*s[1] + bq->b1[1]*s2[1];
                    p1[2]       = bq->a1[2]*s[2] + bq->b1[2]*s2[2];
                    p1[3]       = bq->a1[3]*s[3] + bq->b1[3]*s2[3];

                    p2[0]       = bq->a2[0]*s[0] + bq->b2[0]*s2[0];
                    p2[1]       = bq->a2[1]*s[1] + bq->b2[1]*s2[1];
                    p2[2]       = bq->a2[2]*s[2] + bq->b2[2]*s2[2];
                    p2[3]       = bq->a2[3]*s[3] + bq->b2[3]*s2[3];

                    f->d[0]     = f->d[8]   + p1[0];
                    f->d[1]     = f->d[9]   + p1[1];
                    f->d[2]     = f->d[10]  + p1[2];
                    f->d[3]     = f->d[11]  + p1[3];

                    f->d[8]     = p2[0];
                    f->d[9]     = p2[1];
                    f->d[10]    = p2[2];
                    f->d[11]    = p2[3];

                    // Shift buffer
                    *(dp++)     = s2[3];
                    s[3]        = s2[2];
                    s[2]        = s2[1];
                    s[1]        = s2[0];

                    i          ++;
                }

                // Finish processing
                do
                {
                    // Calculate filters by mask and shift buffers
                    if (mask & 0x2)
                    {
                        s2[1]       = bq->a0[1]*s[1] + f->d[1];
                        p1[1]       = bq->a1[1]*s[1] + bq->b1[1]*s2[1];
                        p2[1]       = bq->a2[1]*s[1] + bq->b2[1]*s2[1];
                        f->d[1]     = f->d[9]   + p1[1];
                        f->d[9]     = p2[1];
                    }
                    if (mask & 0x4)
                    {
                        s2[2]       = bq->a0[2]*s[2] + f->d[2];
                        p1[2]       = bq->a1[2]*s[2] + bq->b1[2]*s2[2];
                        p2[2]       = bq->a2[2]*s[2] + bq->b2[2]*s2[2];
                        f->d[2]     = f->d[10]  + p1[2];
                        f->d[10]    = p2[2];
                    }

                    s2[3]       = bq->a0[3]*s[3] + f->d[3];
                    p1[3]       = bq->a1[3]*s[3] + bq->b1[3]*s2[3];
                    p2[3]       = bq->a2[3]*s[3] + bq->b2[3]*s2[3];
                    f->d[3]     = f->d[11]  + p1[3];
                    f->d[11]    = p2[3];

                    // Shift buffer
                    *(dp++)     = s2[3];
                    s[3]        = s2[2];
                    s[2]        = s2[1];
                    s[1]        = s2[0];

                    // Update mask
                    mask        = (mask << 1) & 0x0f;
                } while (mask != 0);
            }
        }

    }
}


#endif /* CORE_NATIVE_FILTERS_H_ */