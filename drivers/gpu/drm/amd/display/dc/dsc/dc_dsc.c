/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author: AMD
 */

#ifdef CONFIG_DRM_AMD_DC_DSC_SUPPORT
#include "dc.h"
#include "core_types.h"
#include "dsc.h"
#include <drm/drm_dp_helper.h>

/* This module's internal functions */

static bool dsc_buff_block_size_from_dpcd(int dpcd_buff_block_size, int *buff_block_size)
{

	switch (dpcd_buff_block_size) {
	case DP_DSC_RC_BUF_BLK_SIZE_1:
		*buff_block_size = 1024;
		break;
	case DP_DSC_RC_BUF_BLK_SIZE_4:
		*buff_block_size = 4 * 1024;
		break;
	case DP_DSC_RC_BUF_BLK_SIZE_16:
		*buff_block_size = 16 * 1024;
		break;
	case DP_DSC_RC_BUF_BLK_SIZE_64:
		*buff_block_size = 64 * 1024;
		break;
	default: {
			dm_error("%s: DPCD DSC buffer size not recoginzed.\n", __func__);
			return false;
		}
	}

	return true;
}


static bool dsc_line_buff_depth_from_dpcd(int dpcd_line_buff_bit_depth, int *line_buff_bit_depth)
{
	if (0 <= dpcd_line_buff_bit_depth && dpcd_line_buff_bit_depth <= 7)
		*line_buff_bit_depth = dpcd_line_buff_bit_depth + 9;
	else if (dpcd_line_buff_bit_depth == 8)
		*line_buff_bit_depth = 8;
	else {
		dm_error("%s: DPCD DSC buffer depth not recoginzed.\n", __func__);
		return false;
	}

	return true;
}


static bool dsc_throughput_from_dpcd(int dpcd_throughput, int *throughput)
{
	switch (dpcd_throughput) {
	case DP_DSC_THROUGHPUT_MODE_0_340:
		*throughput = 340;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_400:
		*throughput = 400;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_450:
		*throughput = 450;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_500:
		*throughput = 500;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_550:
		*throughput = 550;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_600:
		*throughput = 600;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_650:
		*throughput = 650;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_700:
		*throughput = 700;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_750:
		*throughput = 750;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_800:
		*throughput = 800;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_850:
		*throughput = 850;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_900:
		*throughput = 900;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_950:
		*throughput = 950;
		break;
	case DP_DSC_THROUGHPUT_MODE_0_1000:
		*throughput = 1000;
		break;
	default: {
			dm_error("%s: DPCD DSC througput mode not recoginzed.\n", __func__);
			return false;
		}
	}

	return true;
}


static bool dsc_bpp_increment_div_from_dpcd(int bpp_increment_dpcd, uint32_t *bpp_increment_div)
{

	switch (bpp_increment_dpcd) {
	case 0:
		*bpp_increment_div = 16;
		break;
	case 1:
		*bpp_increment_div = 8;
		break;
	case 2:
		*bpp_increment_div = 4;
		break;
	case 3:
		*bpp_increment_div = 2;
		break;
	case 4:
		*bpp_increment_div = 1;
		break;
	default: {
		dm_error("%s: DPCD DSC bits-per-pixel increment not recoginzed.\n", __func__);
		return false;
	}
	}

	return true;
}

static void get_dsc_enc_caps(
	const struct dc *dc,
	struct dsc_enc_caps *dsc_enc_caps,
	int pixel_clock_100Hz)
{
	// This is a static HW query, so we can use any DSC
	struct display_stream_compressor *dsc = dc->res_pool->dscs[0];

	memset(dsc_enc_caps, 0, sizeof(struct dsc_enc_caps));
	if (dsc)
		dsc->funcs->dsc_get_enc_caps(dsc_enc_caps, pixel_clock_100Hz);
}

/* Returns 'false' if no intersection was found for at least one capablity.
 * It also implicitly validates some sink caps against invalid value of zero.
 */
static bool dc_intersect_dsc_caps(
	const struct dsc_dec_dpcd_caps *dsc_sink_caps,
	const struct dsc_enc_caps *dsc_enc_caps,
	enum dc_pixel_encoding pixel_encoding,
	struct dsc_enc_caps *dsc_common_caps)
{
	int32_t max_slices;
	int32_t total_sink_throughput;

	memset(dsc_common_caps, 0, sizeof(struct dsc_enc_caps));

	dsc_common_caps->dsc_version = min(dsc_sink_caps->dsc_version, dsc_enc_caps->dsc_version);
	if (!dsc_common_caps->dsc_version)
		return false;

	dsc_common_caps->slice_caps.bits.NUM_SLICES_1 = dsc_sink_caps->slice_caps1.bits.NUM_SLICES_1 && dsc_enc_caps->slice_caps.bits.NUM_SLICES_1;
	dsc_common_caps->slice_caps.bits.NUM_SLICES_2 = dsc_sink_caps->slice_caps1.bits.NUM_SLICES_2 && dsc_enc_caps->slice_caps.bits.NUM_SLICES_2;
	dsc_common_caps->slice_caps.bits.NUM_SLICES_4 = dsc_sink_caps->slice_caps1.bits.NUM_SLICES_4 && dsc_enc_caps->slice_caps.bits.NUM_SLICES_4;
	dsc_common_caps->slice_caps.bits.NUM_SLICES_8 = dsc_sink_caps->slice_caps1.bits.NUM_SLICES_8 && dsc_enc_caps->slice_caps.bits.NUM_SLICES_8;
	if (!dsc_common_caps->slice_caps.raw)
		return false;

	dsc_common_caps->lb_bit_depth = min(dsc_sink_caps->lb_bit_depth, dsc_enc_caps->lb_bit_depth);
	if (!dsc_common_caps->lb_bit_depth)
		return false;

	dsc_common_caps->is_block_pred_supported = dsc_sink_caps->is_block_pred_supported && dsc_enc_caps->is_block_pred_supported;

	dsc_common_caps->color_formats.raw = dsc_sink_caps->color_formats.raw & dsc_enc_caps->color_formats.raw;
	if (!dsc_common_caps->color_formats.raw)
		return false;

	dsc_common_caps->color_depth.raw = dsc_sink_caps->color_depth.raw & dsc_enc_caps->color_depth.raw;
	if (!dsc_common_caps->color_depth.raw)
		return false;

	max_slices = 0;
	if (dsc_common_caps->slice_caps.bits.NUM_SLICES_1)
		max_slices = 1;

	if (dsc_common_caps->slice_caps.bits.NUM_SLICES_2)
		max_slices = 2;

	if (dsc_common_caps->slice_caps.bits.NUM_SLICES_4)
		max_slices = 4;

	total_sink_throughput = max_slices * dsc_sink_caps->throughput_mode_0_mps;
	if (pixel_encoding == PIXEL_ENCODING_YCBCR422 || pixel_encoding == PIXEL_ENCODING_YCBCR420)
		total_sink_throughput = max_slices * dsc_sink_caps->throughput_mode_1_mps;

	dsc_common_caps->max_total_throughput_mps = min(total_sink_throughput, dsc_enc_caps->max_total_throughput_mps);

	dsc_common_caps->max_slice_width = min(dsc_sink_caps->max_slice_width, dsc_enc_caps->max_slice_width);
	if (!dsc_common_caps->max_slice_width)
		return false;

	dsc_common_caps->bpp_increment_div = min(dsc_sink_caps->bpp_increment_div, dsc_enc_caps->bpp_increment_div);

	return true;
}

// TODO DSC: Can this be moved to a common helper module and replace WindowsDM::calcRequiredBandwidthForTiming()?
static int bpp_from_dc_color_depth(enum dc_color_depth color_depth)
{
	int bits_per_pixel;

	// Get color depth in bits per pixel
	switch (color_depth) {
	case COLOR_DEPTH_UNDEFINED:
		bits_per_pixel = 0;
		break;
	case COLOR_DEPTH_666:
		bits_per_pixel = 6;
		break;
	case COLOR_DEPTH_888:
		bits_per_pixel = 8;
		break;
	case COLOR_DEPTH_101010:
		bits_per_pixel = 10;
		break;
	case COLOR_DEPTH_121212:
		bits_per_pixel = 12;
		break;
	case COLOR_DEPTH_141414:
		bits_per_pixel = 14;
		break;
	case COLOR_DEPTH_161616:
		bits_per_pixel = 16;
		break;
	case COLOR_DEPTH_999:
		bits_per_pixel = 9;
		break;
	case COLOR_DEPTH_111111:
		bits_per_pixel = 11;
		break;
	case COLOR_DEPTH_COUNT:
	default:
		bits_per_pixel = 0;
		break;
	}

	return bits_per_pixel;
}

// TODO DSC: Can this be moved to a common helper module and replace WindowsDM::calcRequiredBandwidthForTiming()?
static int calc_required_bandwidth_for_timing(const struct dc_crtc_timing *crtc_timing)
{
	int timing_bandwidth_kbps = 0;
	int bits_per_pixel = bpp_from_dc_color_depth(crtc_timing->display_color_depth);

	if (crtc_timing->pixel_encoding == PIXEL_ENCODING_RGB ||
		crtc_timing->pixel_encoding == PIXEL_ENCODING_YCBCR444)
		timing_bandwidth_kbps = crtc_timing->pix_clk_100hz * bits_per_pixel * 3 / 10;
	else if (crtc_timing->pixel_encoding == PIXEL_ENCODING_YCBCR422)
		timing_bandwidth_kbps = crtc_timing->pix_clk_100hz * 8 * 3 / 10;
	else if (crtc_timing->pixel_encoding == PIXEL_ENCODING_YCBCR420)
		timing_bandwidth_kbps = crtc_timing->pix_clk_100hz * bits_per_pixel * 3 / 20;

	return timing_bandwidth_kbps;
}

struct dc_dsc_policy {
	float max_compression_ratio_legacy;
	float sst_compression_legacy; // Maximum quality if 0.0
	float mst_compression_legacy;
	bool use_min_slices_h;
	int max_slices_h; // Maximum available if 0
	int num_slices_v;
	int max_target_bpp;
	int min_target_bpp; // Minimum target bits per pixel
};

static inline uint32_t dsc_round_up(uint32_t value)
{
	return (value + 9) / 10;
}

static inline uint32_t calc_dsc_bpp_x16(uint32_t stream_bandwidth_kbps, uint32_t pix_clk_100hz, uint32_t bpp_increment_div)
{
	uint32_t dsc_target_bpp_x16;
	float f_dsc_target_bpp;
	float f_stream_bandwidth_100bps = stream_bandwidth_kbps * 10.0f;
	uint32_t precision = bpp_increment_div; // bpp_increment_div is actually precision

	f_dsc_target_bpp = f_stream_bandwidth_100bps / pix_clk_100hz;

	// Round down to the nearest precision stop to bring it into DSC spec range
	dsc_target_bpp_x16 = (uint32_t)(f_dsc_target_bpp * precision);
	dsc_target_bpp_x16 = (dsc_target_bpp_x16 * 16) / precision;

	return dsc_target_bpp_x16;
}

const struct dc_dsc_policy dsc_policy = {
	.max_compression_ratio_legacy = 3.0f, // DSC Policy: Limit compression to 3:1 at most in all cases
	.sst_compression_legacy = 0.0f, // DSC Policy: SST - Maximum quality (0.0)
	.mst_compression_legacy = 3.0f, // DSC Policy: MST - always 3:1 compression
	.use_min_slices_h = true, // DSC Policy: Use minimum number of slices that fits the pixel clock
	.max_slices_h = 0, // DSC Policy: Use max available slices (in our case 4 for or 8, depending on the mode)

	/* DSC Policy: Number of vertical slices set to 2 for no particular reason.
	 * Seems small enough to not affect the quality too much, while still providing some error
	 * propagation control (which may also help debugging).
	 */
	.num_slices_v = 16,
	.max_target_bpp = 24,
	.min_target_bpp = 8,
};

static void get_dsc_bandwidth_range(
		const struct dc_dsc_policy *policy,
		const struct dsc_enc_caps *dsc_caps,
		const struct dc_crtc_timing *timing,
		struct dc_dsc_bw_range *range)
{
	/* native stream bandwidth */
	range->stream_kbps = calc_required_bandwidth_for_timing(timing);

	/* max dsc target bpp */
	range->max_kbps = dsc_round_up(policy->max_target_bpp * timing->pix_clk_100hz);
	range->max_target_bpp_x16 = policy->max_target_bpp * 16;
	if (range->max_kbps > range->stream_kbps) {
		/* max dsc target bpp is capped to native bandwidth */
		range->max_kbps = range->stream_kbps;
		range->max_target_bpp_x16 = calc_dsc_bpp_x16(range->stream_kbps, timing->pix_clk_100hz, dsc_caps->bpp_increment_div);
	}

	/* min dsc target bpp */
	range->min_kbps = dsc_round_up(policy->min_target_bpp * timing->pix_clk_100hz);
	range->min_target_bpp_x16 = policy->min_target_bpp * 16;
	if (range->min_kbps > range->max_kbps) {
		/* min dsc target bpp is capped to max dsc bandwidth*/
		range->min_kbps = range->max_kbps;
		range->min_target_bpp_x16 = range->max_target_bpp_x16;
	}
}

static bool decide_dsc_target_bpp_x16(
		const struct dc_dsc_policy *policy,
		const struct dsc_enc_caps *dsc_common_caps,
		const int target_bandwidth,
		const struct dc_crtc_timing *timing,
		int *target_bpp_x16)
{
	bool should_use_dsc = false;
	struct dc_dsc_bw_range range;
	float target_bandwidth_kbps = target_bandwidth * 0.97f; // 3% overhead for FEC

	memset(&range, 0, sizeof(range));

	get_dsc_bandwidth_range(policy, dsc_common_caps, timing, &range);
	if (target_bandwidth_kbps >= range.stream_kbps) {
		/* enough bandwidth without dsc */
		*target_bpp_x16 = 0;
		should_use_dsc = false;
	} else if (target_bandwidth_kbps >= range.max_kbps) {
		/* use max target bpp allowed */
		*target_bpp_x16 = range.max_target_bpp_x16;
		should_use_dsc = true;
	} else if (target_bandwidth_kbps >= range.min_kbps) {
		/* use target bpp that can take entire target bandwidth */
		*target_bpp_x16 = calc_dsc_bpp_x16(target_bandwidth_kbps, timing->pix_clk_100hz, dsc_common_caps->bpp_increment_div);
		should_use_dsc = true;
	} else {
		/* not enough bandwidth to fulfill minimum requirement */
		*target_bpp_x16 = 0;
		should_use_dsc = false;
	}

	return should_use_dsc;
}


#define MIN_AVAILABLE_SLICES_SIZE  4

static int get_available_dsc_slices(union dsc_enc_slice_caps slice_caps, int *available_slices)
{
	int idx = 0;

	memset(available_slices, -1, MIN_AVAILABLE_SLICES_SIZE);

	if (slice_caps.bits.NUM_SLICES_1)
		available_slices[idx++] = 1;

	if (slice_caps.bits.NUM_SLICES_2)
		available_slices[idx++] = 2;

	if (slice_caps.bits.NUM_SLICES_4)
		available_slices[idx++] = 4;

	if (slice_caps.bits.NUM_SLICES_8)
		available_slices[idx++] = 8;

	return idx;
}


static int get_max_dsc_slices(union dsc_enc_slice_caps slice_caps)
{
	int max_slices = 0;
	int available_slices[MIN_AVAILABLE_SLICES_SIZE];
	int end_idx = get_available_dsc_slices(slice_caps, &available_slices[0]);

	if (end_idx > 0)
		max_slices = available_slices[end_idx - 1];

	return max_slices;
}


// Increment sice number in available sice numbers stops if possible, or just increment if not
static int inc_num_slices(union dsc_enc_slice_caps slice_caps, int num_slices)
{
	// Get next bigger num slices available in common caps
	int available_slices[MIN_AVAILABLE_SLICES_SIZE];
	int end_idx;
	int i;
	int new_num_slices = num_slices;

	end_idx = get_available_dsc_slices(slice_caps, &available_slices[0]);
	if (end_idx == 0) {
		// No available slices found
		new_num_slices++;
		return new_num_slices;
	}

	// Numbers of slices found - get the next bigger number
	for (i = 0; i < end_idx; i++) {
		if (new_num_slices < available_slices[i]) {
			new_num_slices = available_slices[i];
			break;
		}
	}

	if (new_num_slices == num_slices) // No biger number of slices found
		new_num_slices++;

	return new_num_slices;
}


// Decrement sice number in available sice numbers stops if possible, or just decrement if not. Stop at zero.
static int dec_num_slices(union dsc_enc_slice_caps slice_caps, int num_slices)
{
	// Get next bigger num slices available in common caps
	int available_slices[MIN_AVAILABLE_SLICES_SIZE];
	int end_idx;
	int i;
	int new_num_slices = num_slices;

	end_idx = get_available_dsc_slices(slice_caps, &available_slices[0]);
	if (end_idx == 0 && new_num_slices > 0) {
		// No numbers of slices found
		new_num_slices++;
		return new_num_slices;
	}

	// Numbers of slices found - get the next smaller number
	for (i = end_idx - 1; i >= 0; i--) {
		if (new_num_slices > available_slices[i]) {
			new_num_slices = available_slices[i];
			break;
		}
	}

	if (new_num_slices == num_slices) {
		// No smaller number of slices found
		new_num_slices--;
		if (new_num_slices < 0)
			new_num_slices = 0;
	}

	return new_num_slices;
}


// Choose next bigger number of slices if the requested number of slices is not available
static int fit_num_slices_up(union dsc_enc_slice_caps slice_caps, int num_slices)
{
	// Get next bigger num slices available in common caps
	int available_slices[MIN_AVAILABLE_SLICES_SIZE];
	int end_idx;
	int i;
	int new_num_slices = num_slices;

	end_idx = get_available_dsc_slices(slice_caps, &available_slices[0]);
	if (end_idx == 0) {
		// No available slices found
		new_num_slices++;
		return new_num_slices;
	}

	// Numbers of slices found - get the equal or next bigger number
	for (i = 0; i < end_idx; i++) {
		if (new_num_slices <= available_slices[i]) {
			new_num_slices = available_slices[i];
			break;
		}
	}

	return new_num_slices;
}


/* Attempts to set DSC configuration for the stream, applying DSC policy.
 * Returns 'true' if successful or 'false' if not.
 *
 * Parameters:
 *
 * dsc_sink_caps       - DSC sink decoder capabilities (from DPCD)
 *
 * dsc_enc_caps        - DSC encoder capabilities
 *
 * target_bandwidth    - Target bandwidth to fit the stream into.
 *                       If 0, use maximum compression as per DSC policy.
 *
 * timing              - The stream timing to fit into 'target_bandwidth' or apply
 *                       maximum compression to, if 'target_badwidth == 0'
 *
 * dsc_cfg             - DSC configuration to use if it was possible to come up with
 *                       one for the given inputs.
 *                       The target bitrate after DSC can be calculated by multiplying
 *                       dsc_cfg.bits_per_pixel (in U6.4 format) by pixel rate, e.g.
 *
 *                       dsc_stream_bitrate_kbps = (int)ceil(timing->pix_clk_khz * dsc_cfg.bits_per_pixel / 16.0);
 */
static bool setup_dsc_config(
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		const struct dsc_enc_caps *dsc_enc_caps,
		int target_bandwidth,
		const struct dc_crtc_timing *timing,
		struct dc_dsc_config *dsc_cfg)
{
	struct dsc_enc_caps dsc_common_caps;
	int max_slices_h;
	int min_slices_h;
	int num_slices_h;
	int pic_width;
	int slice_width;
	int target_bpp;
	int sink_per_slice_throughput;
	// TODO DSC: See if it makes sense to use 2.4% for SST
	bool is_dsc_possible = false;
	int num_slices_v;
	int pic_height;

	memset(dsc_cfg, 0, sizeof(struct dc_dsc_config));

	if (!dsc_sink_caps->is_dsc_supported)
		goto done;

	// Intersect decoder with encoder DSC caps and validate DSC settings
	is_dsc_possible = dc_intersect_dsc_caps(dsc_sink_caps, dsc_enc_caps, timing->pixel_encoding, &dsc_common_caps);
	if (!is_dsc_possible)
		goto done;

	if (target_bandwidth > 0) {
		is_dsc_possible = decide_dsc_target_bpp_x16(&dsc_policy, &dsc_common_caps, target_bandwidth, timing, &target_bpp);
	} else if (timing->pix_clk_100hz * 12 <= calc_required_bandwidth_for_timing(timing) * 10) {
		/* use 12 target bpp for MST display
		 * TODO: implement new MST DSC target bpp policy */
		target_bpp = 16*12;
		is_dsc_possible = true;
	} else {
		is_dsc_possible = false;
	}

	if (!is_dsc_possible)
		goto done;

	dsc_cfg->bits_per_pixel = target_bpp;

	sink_per_slice_throughput = 0;

	// Validate available DSC settings against the mode timing

	// Color format
	dsc_cfg->ycbcr422_simple = false;
	switch (timing->pixel_encoding)	{
	case PIXEL_ENCODING_RGB:
		is_dsc_possible = (bool)dsc_common_caps.color_formats.bits.RGB;
		sink_per_slice_throughput = dsc_sink_caps->throughput_mode_0_mps;
		break;
	case PIXEL_ENCODING_YCBCR444:
		is_dsc_possible = (bool)dsc_common_caps.color_formats.bits.YCBCR_444;
		sink_per_slice_throughput = dsc_sink_caps->throughput_mode_0_mps;
		break;
	case PIXEL_ENCODING_YCBCR422: {
			is_dsc_possible = (bool)dsc_common_caps.color_formats.bits.YCBCR_NATIVE_422;
			sink_per_slice_throughput = dsc_sink_caps->throughput_mode_1_mps;
			if (!is_dsc_possible) {
				is_dsc_possible = (bool)dsc_common_caps.color_formats.bits.YCBCR_SIMPLE_422;
				dsc_cfg->ycbcr422_simple = is_dsc_possible;
				sink_per_slice_throughput = dsc_sink_caps->throughput_mode_0_mps;
			}
		}
		break;
	case PIXEL_ENCODING_YCBCR420:
		is_dsc_possible = (bool)dsc_common_caps.color_formats.bits.YCBCR_NATIVE_420;
		sink_per_slice_throughput = dsc_sink_caps->throughput_mode_1_mps;
		break;
	default:
		is_dsc_possible = false;
	}

	if (!is_dsc_possible)
		goto done;

	// Color depth
	switch (timing->display_color_depth) {
	case COLOR_DEPTH_888:
		is_dsc_possible = (bool)dsc_common_caps.color_depth.bits.COLOR_DEPTH_8_BPC;
		break;
	case COLOR_DEPTH_101010:
		is_dsc_possible = (bool)dsc_common_caps.color_depth.bits.COLOR_DEPTH_10_BPC;
		break;
	case COLOR_DEPTH_121212:
		is_dsc_possible = (bool)dsc_common_caps.color_depth.bits.COLOR_DEPTH_12_BPC;
		break;
	default:
		is_dsc_possible = false;
	}

	if (!is_dsc_possible)
		goto done;

	// DSC slicing
	max_slices_h = get_max_dsc_slices(dsc_common_caps.slice_caps);

	pic_width = timing->h_addressable + timing->h_border_left + timing->h_border_right;
	while (max_slices_h > 0) {
		if (pic_width % max_slices_h == 0)
			break;

		max_slices_h = dec_num_slices(dsc_common_caps.slice_caps, max_slices_h);
	}

	is_dsc_possible = (dsc_common_caps.max_slice_width > 0);
	if (!is_dsc_possible)
		goto done;

	min_slices_h = pic_width / dsc_common_caps.max_slice_width;
	if (pic_width % dsc_common_caps.max_slice_width)
		min_slices_h++;

	min_slices_h = fit_num_slices_up(dsc_common_caps.slice_caps, min_slices_h);

	while (min_slices_h <= max_slices_h) {
		if (dsc_round_up(timing->pix_clk_100hz) / (min_slices_h) <= sink_per_slice_throughput * 1000)
			break;

		min_slices_h = inc_num_slices(dsc_common_caps.slice_caps, min_slices_h);
	}

	if (pic_width % min_slices_h != 0)
		min_slices_h = 0; // DSC TODO: Maybe try increasing the number of slices first?

	is_dsc_possible = (min_slices_h <= max_slices_h);
	if (!is_dsc_possible)
		goto done;

	if (dsc_policy.use_min_slices_h) {
		if (min_slices_h > 0)
			num_slices_h = min_slices_h;
		else if (max_slices_h > 0) { // Fall back to max slices if min slices is not working out
			if (dsc_policy.max_slices_h)
				num_slices_h = min(dsc_policy.max_slices_h, max_slices_h);
			else
				num_slices_h = max_slices_h;
		} else
			is_dsc_possible = false;
	} else {
		if (max_slices_h > 0) {
			if (dsc_policy.max_slices_h)
				num_slices_h = min(dsc_policy.max_slices_h, max_slices_h);
			else
				num_slices_h = max_slices_h;
		} else if (min_slices_h > 0) // Fall back to min slices if max slices is not possible
			num_slices_h = min_slices_h;
		else
			is_dsc_possible = false;
	}

	if (!is_dsc_possible)
		goto done;

	dsc_cfg->num_slices_h = num_slices_h;
	slice_width = pic_width / num_slices_h;

	// Vertical number of slices: start from policy and pick the first one that height is divisible by
	pic_height = timing->v_addressable + timing->v_border_top + timing->v_border_bottom;
	num_slices_v = dsc_policy.num_slices_v;
	if (num_slices_v < 1)
		num_slices_v = 1;

	while (num_slices_v >= 1 && (pic_height % num_slices_v != 0))
		num_slices_v--;

	dsc_cfg->num_slices_v = num_slices_v;

	is_dsc_possible = slice_width <= dsc_common_caps.max_slice_width;
	if (!is_dsc_possible)
		goto done;

	// Final decission: can we do DSC or not?
	if (is_dsc_possible) {
		// Fill out the rest of DSC settings
		dsc_cfg->block_pred_enable = dsc_common_caps.is_block_pred_supported;
		dsc_cfg->linebuf_depth = dsc_common_caps.lb_bit_depth;
		dsc_cfg->version_minor = (dsc_common_caps.dsc_version & 0xf0) >> 4;
	}

done:
	if (!is_dsc_possible)
		memset(dsc_cfg, 0, sizeof(struct dc_dsc_config));

	return is_dsc_possible;
}

bool dc_dsc_parse_dsc_dpcd(const uint8_t *dpcd_dsc_data, struct dsc_dec_dpcd_caps *dsc_sink_caps)
{
	dsc_sink_caps->is_dsc_supported = (dpcd_dsc_data[DP_DSC_SUPPORT - DP_DSC_SUPPORT] & DP_DSC_DECOMPRESSION_IS_SUPPORTED) != 0;
	if (!dsc_sink_caps->is_dsc_supported)
		return true;

	dsc_sink_caps->dsc_version = dpcd_dsc_data[DP_DSC_REV - DP_DSC_SUPPORT];

	{
		int buff_block_size;
		int buff_size;

		if (!dsc_buff_block_size_from_dpcd(dpcd_dsc_data[DP_DSC_RC_BUF_BLK_SIZE - DP_DSC_SUPPORT], &buff_block_size))
			return false;

		buff_size = dpcd_dsc_data[DP_DSC_RC_BUF_SIZE - DP_DSC_SUPPORT] + 1;
		dsc_sink_caps->rc_buffer_size = buff_size * buff_block_size;
	}

	dsc_sink_caps->slice_caps1.raw = dpcd_dsc_data[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT];
	if (!dsc_line_buff_depth_from_dpcd(dpcd_dsc_data[DP_DSC_LINE_BUF_BIT_DEPTH - DP_DSC_SUPPORT], &dsc_sink_caps->lb_bit_depth))
		return false;

	dsc_sink_caps->is_block_pred_supported =
		(dpcd_dsc_data[DP_DSC_BLK_PREDICTION_SUPPORT - DP_DSC_SUPPORT] & DP_DSC_BLK_PREDICTION_IS_SUPPORTED) != 0;

	dsc_sink_caps->edp_max_bits_per_pixel =
		dpcd_dsc_data[DP_DSC_MAX_BITS_PER_PIXEL_LOW - DP_DSC_SUPPORT] |
		dpcd_dsc_data[DP_DSC_MAX_BITS_PER_PIXEL_HI - DP_DSC_SUPPORT] << 8;

	dsc_sink_caps->color_formats.raw = dpcd_dsc_data[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT];
	dsc_sink_caps->color_depth.raw = dpcd_dsc_data[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT];

	{
		int dpcd_throughput = dpcd_dsc_data[DP_DSC_PEAK_THROUGHPUT - DP_DSC_SUPPORT];

		if (!dsc_throughput_from_dpcd(dpcd_throughput & DP_DSC_THROUGHPUT_MODE_0_MASK, &dsc_sink_caps->throughput_mode_0_mps))
			return false;

		dpcd_throughput = (dpcd_throughput & DP_DSC_THROUGHPUT_MODE_1_MASK) >> DP_DSC_THROUGHPUT_MODE_1_SHIFT;
		if (!dsc_throughput_from_dpcd(dpcd_throughput, &dsc_sink_caps->throughput_mode_1_mps))
			return false;
	}

	dsc_sink_caps->max_slice_width = dpcd_dsc_data[DP_DSC_MAX_SLICE_WIDTH - DP_DSC_SUPPORT] * 320;
	dsc_sink_caps->slice_caps2.raw = dpcd_dsc_data[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT];

	if (!dsc_bpp_increment_div_from_dpcd(dpcd_dsc_data[DP_DSC_BITS_PER_PIXEL_INC - DP_DSC_SUPPORT], &dsc_sink_caps->bpp_increment_div))
		return false;

	return true;
}


bool dc_dsc_compute_bandwidth_range(
		const struct dc *dc,
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		const struct dc_crtc_timing *timing,
		struct dc_dsc_bw_range *range)
{
	bool is_dsc_possible = false;
	struct dsc_enc_caps dsc_enc_caps;
	struct dsc_enc_caps dsc_common_caps;
	get_dsc_enc_caps(dc, &dsc_enc_caps, timing->pix_clk_100hz);
	is_dsc_possible = dc_intersect_dsc_caps(dsc_sink_caps, &dsc_enc_caps, timing->pixel_encoding, &dsc_common_caps);
	if (is_dsc_possible)
		get_dsc_bandwidth_range(&dsc_policy, &dsc_common_caps, timing, range);
	return is_dsc_possible;
}

bool dc_dsc_compute_config(
		const struct dc *dc,
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		int target_bandwidth,
		const struct dc_crtc_timing *timing,
		struct dc_dsc_config *dsc_cfg)
{
	bool is_dsc_possible = false;

	struct dsc_enc_caps dsc_enc_caps;
	struct dsc_enc_caps dsc_common_caps;

	get_dsc_enc_caps(dc, &dsc_enc_caps, timing->pix_clk_100hz);
	is_dsc_possible = dc_intersect_dsc_caps(dsc_sink_caps, &dsc_enc_caps,
			timing->pixel_encoding, &dsc_common_caps);
	if (is_dsc_possible)
		is_dsc_possible = setup_dsc_config(dsc_sink_caps,
				&dsc_enc_caps,
				target_bandwidth,
				timing, dsc_cfg);
	return is_dsc_possible;
}

bool dc_check_and_fit_timing_into_bandwidth_with_dsc_legacy(
		const struct dc *pDC,
		const struct dc_link *link,
		struct dc_crtc_timing *timing)
{
	int requiredBandwidth_Kbps;
	bool stream_fits_into_bandwidth = false;
	int total_link_bandwdith_kbps = dc_link_bandwidth_kbps(link, &link->verified_link_cap);

	if (link->preferred_link_setting.lane_count != LANE_COUNT_UNKNOWN &&
			link->preferred_link_setting.link_rate != LINK_RATE_UNKNOWN) {
		total_link_bandwdith_kbps = dc_link_bandwidth_kbps(link, &link->preferred_link_setting);
	}

	timing->flags.DSC = 0;
	requiredBandwidth_Kbps = calc_required_bandwidth_for_timing(timing);

	if (total_link_bandwdith_kbps >= requiredBandwidth_Kbps)
		stream_fits_into_bandwidth = true;
	else {
		// There's not enough bandwidth in the link. See if DSC can be used to resolve this.
		int link_bandwidth_kbps = link->type == dc_connection_mst_branch ? 0 : total_link_bandwdith_kbps;

		stream_fits_into_bandwidth = dc_setup_dsc_in_timing_legacy(pDC, &link->dpcd_caps.dsc_sink_caps, link_bandwidth_kbps, timing);
	}

	return stream_fits_into_bandwidth;
}

bool dc_setup_dsc_in_timing_legacy(const struct dc *pDC,
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		int available_bandwidth_kbps,
		struct dc_crtc_timing *timing)
{
	bool isDscOK = false;
	struct dsc_enc_caps dsc_enc_caps;

	timing->flags.DSC = 0;
	get_dsc_enc_caps(pDC, &dsc_enc_caps, timing->pix_clk_100hz);
	if (dsc_enc_caps.dsc_version) {
		struct dc_dsc_config dscCfg = {0};

		isDscOK = setup_dsc_config(dsc_sink_caps, &dsc_enc_caps, available_bandwidth_kbps, timing, &dscCfg);

		memcpy(&timing->dsc_cfg, &dscCfg, sizeof(dscCfg));
		timing->flags.DSC = isDscOK ? 1 : 0;
	}

	return isDscOK;
}
#endif /* CONFIG_DRM_AMD_DC_DSC_SUPPORT */