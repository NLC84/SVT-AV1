/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file PackUnPackTest.cc
 *
 * @brief Unit test for Pack UnPack functions:
 * - c_pack_avx2_intrin
 * - eb_enc_msb_pack2d_avx2_intrin_al
 * - eb_enc_msb_pack2d_sse2_intrin
 * - compressed_packmsb_avx2_intrin
 * - eb_enc_un_pack8_bit_data_avx2_intrin
 * - eb_enc_msb_un_pack2d_sse2_intrin
 * - unpack_avg_avx2_intrin
 * - unpack_avg_sse2_intrin
 * - unpack_avg_safe_sub_avx2_intrin
 *
 * @author Cidana-Ivy, Cidana-Wenyao
 *
 ******************************************************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <new>
// workaround to eliminate the compiling warning on linux
// The macro will conflict with definition in gtest.h
#ifdef __USE_GNU
#undef __USE_GNU  // defined in EbThreads.h
#endif
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE  // defined in EbThreads.h
#endif

#include "EbDefinitions.h"
#include "EbPackUnPack_AVX2.h"
#include "EbPictureOperators_AVX2.h"
#include "EbPackUnPack_SSE2.h"
#include "EbPackUnPack_C.h"
#include "EbIntraPrediction.h"
#include "EbUnitTestUtility.h"
#include "EbUtility.h"
#include "random.h"
#include "util.h"

using svt_av1_test_tool::SVTRandom;  // to generate the random

namespace {
const int RANDOM_TIME = 8;
typedef std::tuple<uint32_t, uint32_t> AreaSize;
/*
 * TEST_PACK_SIZES: containing width of 32, 64.
 */
AreaSize TEST_PACK_SIZES[] = {AreaSize(32, 32),
                              AreaSize(32, 8),
                              AreaSize(32, 16),
                              AreaSize(64, 16),
                              AreaSize(64, 64),
                              AreaSize(64, 32),
                              AreaSize(32, 64)};

// test c_pack_avx2_intrin, which only support width of 32 and 64;
class PackTest : public ::testing::Test,
                 public ::testing::WithParamInterface<AreaSize> {
  public:
    PackTest()
        : area_width_(std::get<0>(GetParam())),
          area_height_(std::get<1>(GetParam())) {
        in_stride_ = MAX_SB_SIZE;
        out_stride_ = MAX_SB_SIZE >> 2;
        test_size_ = MAX_SB_SQUARE;
        in_compn_bit_buffer1_ = nullptr;
        in_compn_bit_buffer2_ = nullptr;
        in_bit_buffer_ = nullptr;
        local_cache1_ = nullptr;
        local_cache2_ = nullptr;
    }

    void SetUp() override {
        in_compn_bit_buffer1_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        in_compn_bit_buffer2_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        in_bit_buffer_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        local_cache1_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        local_cache2_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        memset(in_compn_bit_buffer1_,
               0,
               test_size_ * sizeof(in_compn_bit_buffer1_[0]));
        memset(in_compn_bit_buffer2_,
               0,
               test_size_ * sizeof(in_compn_bit_buffer2_[0]));
        memset(local_cache1_, 0, test_size_ * sizeof(local_cache1_[0]));
        memset(local_cache2_, 0, test_size_ * sizeof(local_cache2_[0]));
    }

    void TearDown() override {
        if (in_bit_buffer_)
            eb_aom_free(in_bit_buffer_);
        if (in_compn_bit_buffer1_)
            eb_aom_free(in_compn_bit_buffer1_);
        if (in_compn_bit_buffer2_)
            eb_aom_free(in_compn_bit_buffer2_);
        if (local_cache1_)
            eb_aom_free(local_cache1_);
        if (local_cache2_)
            eb_aom_free(local_cache2_);
    }

  protected:
    void check_output(uint32_t width, uint32_t height, uint8_t *out_1,
                      uint8_t *out_2) {
        int fail_count = 0;
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width / 4; k++) {
                if (out_1[k + j * out_stride_] != out_2[k + j * out_stride_])
                    fail_count++;
            }
        }
        EXPECT_EQ(0, fail_count)
            << "compare result error"
            << "in test area for " << fail_count << "times";
    }

    void run_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            eb_buf_random_u8(in_bit_buffer_, test_size_);
            c_pack_avx2_intrin(in_bit_buffer_,
                               in_stride_,
                               in_compn_bit_buffer1_,
                               out_stride_,
                               local_cache1_,
                               area_width_,
                               area_height_);
            c_pack_c(in_bit_buffer_,
                     in_stride_,
                     in_compn_bit_buffer2_,
                     out_stride_,
                     local_cache2_,
                     area_width_,
                     area_height_);

            check_output(area_width_,
                         area_height_,
                         in_compn_bit_buffer1_,
                         in_compn_bit_buffer2_);

            EXPECT_FALSE(HasFailure())
                << "c_pack_avx2_intrin failed at " << i << "th test with size ("
                << area_width_ << "," << area_height_ << ")";
        }
    }

    uint8_t *local_cache1_, *local_cache2_;
    uint8_t *in_bit_buffer_;
    uint32_t in_stride_, out_stride_;
    uint8_t *in_compn_bit_buffer1_, *in_compn_bit_buffer2_;
    uint32_t area_width_, area_height_;
    uint32_t test_size_;
};

TEST_P(PackTest, PackTest) {
    run_test();
};

INSTANTIATE_TEST_CASE_P(PACK, PackTest, ::testing::ValuesIn(TEST_PACK_SIZES));

// test compressed_packmsb_avx2_intrin
// only width of 32 and 64 are supported in compressed_packmsb_avx2_intrin.
// Use TEST_PACK_SIZES to test.
class PackMsbTest : public ::testing::Test,
                    public ::testing::WithParamInterface<AreaSize> {
  public:
    PackMsbTest()
        : area_width_(std::get<0>(GetParam())),
          area_height_(std::get<1>(GetParam())) {
        in8_stride_ = out_stride_ = MAX_PU_SIZE;
        inn_stride = in8_stride_ >> 2;
        test_size_ = MAX_PU_SIZE * MAX_PU_SIZE;
        inn_bit_buffer_ = nullptr;
        in_8bit_buffer_ = nullptr;
        out_16bit_buffer1_ = nullptr;
        out_16bit_buffer2_ = nullptr;
    }

    void SetUp() override {
        inn_bit_buffer_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_ >> 4));
        in_8bit_buffer_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_16bit_buffer1_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        out_16bit_buffer2_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        memset(
            out_16bit_buffer1_, 0, test_size_ * sizeof(out_16bit_buffer1_[0]));
        memset(
            out_16bit_buffer2_, 0, test_size_ * sizeof(out_16bit_buffer2_[0]));
    }

    void TearDown() override {
        if (inn_bit_buffer_)
            eb_aom_free(inn_bit_buffer_);
        if (in_8bit_buffer_)
            eb_aom_free(in_8bit_buffer_);
        if (out_16bit_buffer1_)
            eb_aom_free(out_16bit_buffer1_);
        if (out_16bit_buffer2_)
            eb_aom_free(out_16bit_buffer2_);
    }

  protected:
    void check_output(uint32_t width, uint32_t height, uint16_t *out_1,
                      uint16_t *out_2) {
        int fail_count = 0;
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width; k++) {
                if (out_1[k + j * out_stride_] != out_2[k + j * out_stride_])
                    fail_count++;
            }
        }
        EXPECT_EQ(0, fail_count)
            << "compare result error"
            << "in test area for " << fail_count << "times";
    }

    void run_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            eb_buf_random_u8(inn_bit_buffer_, test_size_ >> 4);
            eb_buf_random_u8(in_8bit_buffer_, test_size_);
            compressed_packmsb_avx2_intrin(in_8bit_buffer_,
                                           in8_stride_,
                                           inn_bit_buffer_,
                                           out_16bit_buffer1_,
                                           inn_stride,
                                           out_stride_,
                                           area_width_,
                                           area_height_);

            compressed_packmsb(in_8bit_buffer_,
                               in8_stride_,
                               inn_bit_buffer_,
                               out_16bit_buffer2_,
                               inn_stride,
                               out_stride_,
                               area_width_,
                               area_height_);

            check_output(area_width_,
                         area_height_,
                         out_16bit_buffer1_,
                         out_16bit_buffer2_);

            EXPECT_FALSE(HasFailure())
                << "compressed_packmsb_avx2_intrin failed at " << i
                << "th test with size (" << area_width_ << "," << area_height_
                << ")";
        }
    }

    uint8_t *inn_bit_buffer_, *in_8bit_buffer_;
    uint32_t in8_stride_, inn_stride, out_stride_;
    uint16_t *out_16bit_buffer1_, *out_16bit_buffer2_;
    uint32_t area_width_, area_height_;
    uint32_t test_size_;
};

TEST_P(PackMsbTest, PackMsbTest) {
    run_test();
};

INSTANTIATE_TEST_CASE_P(PACKMSB, PackMsbTest,
                        ::testing::ValuesIn(TEST_PACK_SIZES));

// test eb_enc_msb_pack2d_avx2_intrin_al and eb_enc_msb_pack2d_sse2_intrin.
// There is an implicit assumption that the width should be multiple of 4.
// Also there are special snippet to handle width of {4, 8, 16, 32, 64}, so use
// TEST_COMMON_SIZES to cover all the width;
AreaSize TEST_COMMON_SIZES[] = {
    AreaSize(4, 4),    AreaSize(4, 8),   AreaSize(8, 4),   AreaSize(8, 8),
    AreaSize(16, 16),  AreaSize(4, 16),  AreaSize(16, 4),  AreaSize(16, 8),
    AreaSize(8, 16),   AreaSize(32, 32), AreaSize(32, 8),  AreaSize(16, 32),
    AreaSize(8, 32),   AreaSize(32, 16), AreaSize(16, 64), AreaSize(64, 16),
    AreaSize(64, 64),  AreaSize(64, 32), AreaSize(32, 64), AreaSize(128, 128),
    AreaSize(68, 64),  AreaSize(72, 64), AreaSize(80, 64), AreaSize(96, 64),
    AreaSize(64, 128), AreaSize(128, 64)};

class Pack2dTest : public ::testing::Test,
                   public ::testing::WithParamInterface<AreaSize> {
  public:
    Pack2dTest()
        : area_width_(std::get<0>(GetParam())),
          area_height_(std::get<1>(GetParam())) {
        in_stride_ = out_stride_ = MAX_SB_SIZE;
        test_size_ = MAX_SB_SQUARE;
        in_8bit_buffer_ = nullptr;
        inn_bit_buffer_ = nullptr;
        out_16bit_buffer1_ = nullptr;
        out_16bit_buffer_c_ = nullptr;
        out_16bit_buffer3_ = nullptr;
    }

    void SetUp() override {
        in_8bit_buffer_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        inn_bit_buffer_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_16bit_buffer1_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        out_16bit_buffer_c_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        out_16bit_buffer3_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        memset(
            out_16bit_buffer1_, 0, test_size_ * sizeof(out_16bit_buffer1_[0]));
        memset(out_16bit_buffer_c_,
               0,
               test_size_ * sizeof(out_16bit_buffer_c_[0]));
        memset(
            out_16bit_buffer3_, 0, test_size_ * sizeof(out_16bit_buffer3_[0]));
    }

    void TearDown() override {
        if (in_8bit_buffer_)
            eb_aom_free(in_8bit_buffer_);
        if (inn_bit_buffer_)
            eb_aom_free(inn_bit_buffer_);
        if (out_16bit_buffer1_)
            eb_aom_free(out_16bit_buffer1_);
        if (out_16bit_buffer_c_)
            eb_aom_free(out_16bit_buffer_c_);
        if (out_16bit_buffer3_)
            eb_aom_free(out_16bit_buffer3_);
    }

  protected:
    void check_output(uint32_t width, uint32_t height, uint16_t *out_1,
                      uint16_t *out_2) {
        int fail_count = 0;
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width; k++) {
                if (out_1[k + j * out_stride_] != out_2[k + j * out_stride_])
                    fail_count++;
            }
        }
        EXPECT_EQ(0, fail_count)
            << "compare result error"
            << "in test area for " << fail_count << "times";
    }

    void run_2d_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            eb_buf_random_u8(in_8bit_buffer_, test_size_);
            eb_buf_random_u8(inn_bit_buffer_, test_size_);

            eb_enc_msb_pack2d_avx2_intrin_al(in_8bit_buffer_,
                                             in_stride_,
                                             inn_bit_buffer_,
                                             out_16bit_buffer1_,
                                             out_stride_,
                                             out_stride_,
                                             area_width_,
                                             area_height_);
            eb_enc_msb_pack2_d(in_8bit_buffer_,
                               in_stride_,
                               inn_bit_buffer_,
                               out_16bit_buffer_c_,
                               out_stride_,
                               out_stride_,
                               area_width_,
                               area_height_);
            eb_enc_msb_pack2d_sse2_intrin(in_8bit_buffer_,
                                          in_stride_,
                                          inn_bit_buffer_,
                                          out_16bit_buffer3_,
                                          out_stride_,
                                          out_stride_,
                                          area_width_,
                                          area_height_);

            check_output(area_width_,
                         area_height_,
                         out_16bit_buffer1_,
                         out_16bit_buffer_c_);
            check_output(area_width_,
                         area_height_,
                         out_16bit_buffer3_,
                         out_16bit_buffer_c_);

            EXPECT_FALSE(HasFailure())
                << "eb_enc_msb_pack2d_{sse2,avx2}_intrin failed at " << i
                << "th test with size (" << area_width_ << "," << area_height_
                << ")";
        }
    }

    uint8_t *in_8bit_buffer_, *inn_bit_buffer_;
    uint32_t in_stride_, out_stride_;
    uint16_t *out_16bit_buffer1_, *out_16bit_buffer_c_, *out_16bit_buffer3_;
    uint32_t area_width_, area_height_;
    uint32_t test_size_;
};

TEST_P(Pack2dTest, Pack2dTest) {
    run_2d_test();
};

INSTANTIATE_TEST_CASE_P(PACK2D, Pack2dTest,
                        ::testing::ValuesIn(TEST_COMMON_SIZES));

// test eb_enc_un_pack8_bit_data_avx2_intrin
// Similar assumption that the width is multiple of 4, using
// TEST_COMMON_SIZES to cover all the special width.
class UnPackTest : public ::testing::Test,
                   public ::testing::WithParamInterface<AreaSize> {
  public:
    UnPackTest()
        : area_width_(std::get<0>(GetParam())),
          area_height_(std::get<1>(GetParam())) {
        in_stride_ = out_stride_ = MAX_SB_SIZE;
        test_size_ = MAX_SB_SQUARE;
        out_8bit_buffer1_ = nullptr;
        out_8bit_buffer2_ = nullptr;
        out_nbit_buffer1_ = nullptr;
        out_nbit_buffer2_ = nullptr;
        in_16bit_buffer_ = nullptr;
    }

    void SetUp() override {
        out_8bit_buffer1_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_8bit_buffer2_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_nbit_buffer1_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_nbit_buffer2_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        in_16bit_buffer_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        memset(out_8bit_buffer1_, 0, test_size_ * sizeof(out_8bit_buffer1_[0]));
        memset(out_8bit_buffer2_, 0, test_size_ * sizeof(out_8bit_buffer2_[0]));
        memset(out_nbit_buffer1_, 0, test_size_ * sizeof(out_8bit_buffer1_[0]));
        memset(out_nbit_buffer2_, 0, test_size_ * sizeof(out_8bit_buffer2_[0]));
    }

    void TearDown() override {
        if (in_16bit_buffer_)
            eb_aom_free(in_16bit_buffer_);
        if (out_8bit_buffer1_)
            eb_aom_free(out_8bit_buffer1_);
        if (out_8bit_buffer2_)
            eb_aom_free(out_8bit_buffer2_);
        if (out_nbit_buffer1_)
            eb_aom_free(out_nbit_buffer1_);
        if (out_nbit_buffer2_)
            eb_aom_free(out_nbit_buffer2_);
    }

  protected:
    void check_output(uint32_t width, uint32_t height, uint8_t *out_1,
                      uint8_t *out_2) {
        int fail_count = 0;
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width; k++) {
                if (out_1[k + j * out_stride_] != out_2[k + j * out_stride_])
                    fail_count++;
            }
        }
        EXPECT_EQ(0, fail_count)
            << "compare result error"
            << "in test area for " << fail_count << "times";
    }

    void run_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            eb_buf_random_u16(in_16bit_buffer_, test_size_);
            eb_enc_un_pack8_bit_data_avx2_intrin(in_16bit_buffer_,
                                                 in_stride_,
                                                 out_8bit_buffer1_,
                                                 out_stride_,
                                                 area_width_,
                                                 area_height_);
            un_pack8_bit_data(in_16bit_buffer_,
                              in_stride_,
                              out_8bit_buffer2_,
                              out_stride_,
                              area_width_,
                              area_height_);

            check_output(area_width_,
                         area_height_,
                         out_8bit_buffer1_,
                         out_8bit_buffer2_);

            EXPECT_FALSE(HasFailure())
                << "eb_enc_un_pack8_bit_data_avx2_intrin failed at " << i
                << "th test with size (" << area_width_ << "," << area_height_
                << ")";
        }
    }

    void run_2d_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            eb_buf_random_u16(in_16bit_buffer_, test_size_);
            eb_enc_msb_un_pack2d_sse2_intrin(in_16bit_buffer_,
                                             in_stride_,
                                             out_8bit_buffer1_,
                                             out_nbit_buffer1_,
                                             out_stride_,
                                             out_stride_,
                                             area_width_,
                                             area_height_);
            eb_enc_msb_un_pack2_d(in_16bit_buffer_,
                                  in_stride_,
                                  out_8bit_buffer2_,
                                  out_nbit_buffer2_,
                                  out_stride_,
                                  out_stride_,
                                  area_width_,
                                  area_height_);

            check_output(area_width_,
                         area_height_,
                         out_8bit_buffer1_,
                         out_8bit_buffer2_);
            check_output(area_width_,
                         area_height_,
                         out_nbit_buffer1_,
                         out_nbit_buffer2_);

            EXPECT_FALSE(HasFailure())
                << "eb_enc_msb_un_pack2d_sse2_intrin failed at " << i
                << "th test with size (" << area_width_ << "," << area_height_
                << ")";
        }
    }

    uint16_t *in_16bit_buffer_;
    uint32_t in_stride_, out_stride_;
    uint8_t *out_8bit_buffer1_, *out_8bit_buffer2_, *out_nbit_buffer1_,
        *out_nbit_buffer2_;
    uint32_t area_width_, area_height_;
    uint32_t test_size_;
};

TEST_P(UnPackTest, UnPackTest) {
    run_test();
};

TEST_P(UnPackTest, UnPack2dTest) {
    run_2d_test();
};

INSTANTIATE_TEST_CASE_P(UNPACK, UnPackTest,
                        ::testing::ValuesIn(TEST_COMMON_SIZES));

// test unpack_avg_avx2_intrin
// only width of {4, 8, 16, 32, 64} are implemented in unpack_avg_avx2_intrin.
// only width of {8, 16, 32, 64} are implemented in
// unpack_avg_safe_sub_avx2_intrin. use TEST_AVG_SIZES to cover all the cases.
AreaSize TEST_AVG_SIZES[] = {AreaSize(4, 4),
                             AreaSize(4, 8),
                             AreaSize(8, 4),
                             AreaSize(8, 8),
                             AreaSize(16, 16),
                             AreaSize(4, 16),
                             AreaSize(16, 4),
                             AreaSize(16, 8),
                             AreaSize(8, 16),
                             AreaSize(32, 32),
                             AreaSize(32, 8),
                             AreaSize(16, 32),
                             AreaSize(8, 32),
                             AreaSize(32, 16),
                             AreaSize(16, 64),
                             AreaSize(64, 16),
                             AreaSize(64, 64),
                             AreaSize(64, 32),
                             AreaSize(32, 64)};

class UnPackAvgTest : public ::testing::Test,
                      public ::testing::WithParamInterface<AreaSize> {
  public:
    UnPackAvgTest()
        : area_width_(std::get<0>(GetParam())),
          area_height_(std::get<1>(GetParam())) {
        in_stride_ = out_stride_ = MAX_SB_SIZE;
        test_size_ = MAX_SB_SQUARE;
        out_8bit_buffer1_ = nullptr;
        out_8bit_buffer_c_ = nullptr;
        out_8bit_buffer3_ = nullptr;
        in_16bit_buffer1_ = nullptr;
        in_16bit_buffer2_ = nullptr;
    }

    void SetUp() override {
        out_8bit_buffer1_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_8bit_buffer_c_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        out_8bit_buffer3_ =
            reinterpret_cast<uint8_t *>(eb_aom_memalign(32, test_size_));
        in_16bit_buffer1_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        in_16bit_buffer2_ = reinterpret_cast<uint16_t *>(
            eb_aom_memalign(32, sizeof(uint16_t) * test_size_));
        memset(in_16bit_buffer1_, 0, test_size_ * sizeof(in_16bit_buffer1_[0]));
        memset(in_16bit_buffer2_, 0, test_size_ * sizeof(in_16bit_buffer2_[0]));
        memset(out_8bit_buffer1_, 0, test_size_ * sizeof(out_8bit_buffer1_[0]));
        memset(
            out_8bit_buffer_c_, 0, test_size_ * sizeof(out_8bit_buffer_c_[0]));
        memset(out_8bit_buffer3_, 0, test_size_ * sizeof(out_8bit_buffer3_[0]));
    }

    void TearDown() override {
        if (in_16bit_buffer1_)
            eb_aom_free(in_16bit_buffer1_);
        if (in_16bit_buffer2_)
            eb_aom_free(in_16bit_buffer2_);
        if (out_8bit_buffer1_)
            eb_aom_free(out_8bit_buffer1_);
        if (out_8bit_buffer_c_)
            eb_aom_free(out_8bit_buffer_c_);
        if (out_8bit_buffer3_)
            eb_aom_free(out_8bit_buffer3_);
    }

  protected:
    void prepare_data() {
        // Limit the range to 10bit
        const int16_t mask = (1 << 10) - 1;
        SVTRandom rnd(0, mask);
        for (uint32_t i = 0; i < test_size_; i++) {
            in_16bit_buffer1_[i] = rnd.random();
            in_16bit_buffer2_[i] = rnd.random();
        }
    }

    void check_output(uint32_t width, uint32_t height, uint8_t *out_1,
                      uint8_t *out_2) {
        int fail_count = 0;
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width; k++) {
                if (out_1[k + j * out_stride_] != out_2[k + j * out_stride_])
                    fail_count++;
            }
        }
        EXPECT_EQ(0, fail_count)
            << "compare result error"
            << "in test area for " << fail_count << "times";
    }

    void run_avg_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            prepare_data();
            unpack_avg_avx2_intrin(in_16bit_buffer1_,
                                   in_stride_,
                                   in_16bit_buffer2_,
                                   in_stride_,
                                   out_8bit_buffer1_,
                                   out_stride_,
                                   area_width_,
                                   area_height_);
            unpack_avg(in_16bit_buffer1_,
                       in_stride_,
                       in_16bit_buffer2_,
                       in_stride_,
                       out_8bit_buffer_c_,
                       out_stride_,
                       area_width_,
                       area_height_);
            unpack_avg_sse2_intrin(in_16bit_buffer1_,
                                   in_stride_,
                                   in_16bit_buffer2_,
                                   in_stride_,
                                   out_8bit_buffer3_,
                                   out_stride_,
                                   area_width_,
                                   area_height_);
            check_output(area_width_,
                         area_height_,
                         out_8bit_buffer1_,
                         out_8bit_buffer_c_);
            check_output(area_width_,
                         area_height_,
                         out_8bit_buffer3_,
                         out_8bit_buffer_c_);

            EXPECT_FALSE(HasFailure())
                << "unpack_avg_{sse2,avx2}_intrin failed at " << i
                << "th test with size (" << area_width_ << "," << area_height_
                << ")";
        }
    }

    void run_sub_avg_test() {
        for (int i = 0; i < RANDOM_TIME; i++) {
            if (area_width_ > 4) {
                prepare_data();
                unpack_avg_safe_sub_avx2_intrin(in_16bit_buffer1_,
                                                in_stride_,
                                                in_16bit_buffer2_,
                                                in_stride_,
                                                out_8bit_buffer1_,
                                                out_stride_,
                                                false,
                                                area_width_,
                                                area_height_);
                unpack_avg_safe_sub(in_16bit_buffer1_,
                                    in_stride_,
                                    in_16bit_buffer2_,
                                    in_stride_,
                                    out_8bit_buffer_c_,
                                    out_stride_,
                                    false,
                                    area_width_,
                                    area_height_);

                check_output(area_width_,
                             area_height_,
                             out_8bit_buffer1_,
                             out_8bit_buffer_c_);

                EXPECT_FALSE(HasFailure())
                    << "unpack_avg_safe_sub_avx2_intrin failed at " << i
                    << "th test with size (" << area_width_ << ","
                    << area_height_ << ")";
            }
        }
    }

    uint16_t *in_16bit_buffer1_, *in_16bit_buffer2_;
    uint32_t in_stride_, out_stride_;
    uint8_t *out_8bit_buffer1_, *out_8bit_buffer_c_, *out_8bit_buffer3_;
    uint32_t area_width_, area_height_;
    uint32_t test_size_;
};

TEST_P(UnPackAvgTest, UnPackAvgTest) {
    run_avg_test();
};

TEST_P(UnPackAvgTest, UnPackSubAvgTest) {
    run_sub_avg_test();
};

INSTANTIATE_TEST_CASE_P(UNPACKAVG, UnPackAvgTest,
                        ::testing::ValuesIn(TEST_AVG_SIZES));

}  // namespace
