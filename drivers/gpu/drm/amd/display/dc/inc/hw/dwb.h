/* Copyright 2012-17 Advanced Micro Devices, Inc.
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
 * Authors: AMD
 *
 */

#ifndef __DC_DWBC_H__
#define __DC_DWBC_H__

#include "dc_hw_types.h"


#define DWB_SW_V2	1
#define DWB_MCIF_BUF_COUNT 4

/* forward declaration of mcif_wb struct */
struct mcif_wb;

enum dce_version;

enum dwb_sw_version {
	dwb_ver_1_0 = 1,
};

enum dwb_source {
	dwb_src_scl = 0,	/* for DCE7x/9x, DCN won't support. */
	dwb_src_blnd,		/* for DCE7x/9x */
	dwb_src_fmt,		/* for DCE7x/9x */
#if defined(CONFIG_DRM_AMD_DC_DCN2_0)
	dwb_src_otg0 = 0x100,	/* for DCN1.x/DCN2.x, register: mmDWB_SOURCE_SELECT */
	dwb_src_otg1,		/* for DCN1.x/DCN2.x */
	dwb_src_otg2,		/* for DCN1.x/DCN2.x */
	dwb_src_otg3,		/* for DCN1.x/DCN2.x */
#else
	dwb_src_otg0 = 0x100,	/* for DCN1.x, register: mmDWB_SOURCE_SELECT */
	dwb_src_otg1,		/* for DCN1.x */
	dwb_src_otg2,		/* for DCN1.x */
	dwb_src_otg3,		/* for DCN1.x */
#endif
	dwb_src_mpc0 = 0x200,	/* for DCN2, register: mmMPC_DWB0_MUX, mmMPC_DWB1_MUX, mmMPC_DWB2_MUX */
	dwb_src_mpc1,		/* for DCN2 */
	dwb_src_mpc2,		/* for DCN2 */
	dwb_src_mpc3,		/* for DCN2 */
	dwb_src_mpc4,		/* for DCN2 */
};

#if defined(CONFIG_DRM_AMD_DC_DCN2_0)
/* DCN1.x, DCN2.x support 2 pipes */
#else
/* DCN1.x supports 2 pipes */
#endif
enum dwb_pipe {
	dwb_pipe0 = 0,
#if defined(CONFIG_DRM_AMD_DC_DCN1_0)
	dwb_pipe1,
#endif
	dwb_pipe_max_num,
};

#if defined(CONFIG_DRM_AMD_DC_DCN2_0)
enum dwb_frame_capture_enable {
	DWB_FRAME_CAPTURE_DISABLE = 0,
	DWB_FRAME_CAPTURE_ENABLE = 1,
};

enum dwb_stereo_eye_select {
	DWB_STEREO_EYE_LEFT  = 1,		/* Capture left eye only */
	DWB_STEREO_EYE_RIGHT = 2,		/* Capture right eye only */
};

enum dwb_stereo_type {
	DWB_STEREO_TYPE_FRAME_PACKING = 0,		/* Frame packing */
	DWB_STEREO_TYPE_FRAME_SEQUENTIAL = 3,	/* Frame sequential */
};

enum wbscl_coef_filter_type_sel {
	WBSCL_COEF_LUMA_VERT_FILTER = 0,
	WBSCL_COEF_CHROMA_VERT_FILTER = 1,
	WBSCL_COEF_LUMA_HORZ_FILTER = 2,
	WBSCL_COEF_CHROMA_HORZ_FILTER = 3
};

#endif

#if defined(CONFIG_DRM_AMD_DC_DCN2_0)
struct dwb_stereo_params {
	bool				stereo_enabled;		/* false: normal mode, true: 3D stereo */
	enum dwb_stereo_type		stereo_type;		/* indicates stereo format */
	bool				stereo_polarity;	/* indicates left eye or right eye comes first in stereo mode */
	enum dwb_stereo_eye_select	stereo_eye_select;	/* indicate which eye should be captured */
};

struct dwb_warmup_params {
	bool	warmup_en;	/* false: normal mode, true: enable pattern generator */
	bool	warmup_mode;	/* false: 420, true: 444 */
	bool	warmup_depth;	/* false: 8bit, true: 10bit */
	int	warmup_data;	/* Data to be sent by pattern generator (same for each pixel component) */
	int	warmup_width;	/* Pattern width (pixels) */
	int	warmup_height;	/* Pattern height (lines) */
};
#endif

struct dwb_caps {
	enum dce_version hw_version;	/* DCN engine version. */
	enum dwb_sw_version sw_version;	/* DWB sw implementation version. */
	unsigned int	reserved[6];	/* Reserved for future use, MUST BE 0. */
	unsigned int	adapter_id;
	unsigned int	num_pipes;	/* number of DWB pipes */
	struct {
		unsigned int support_dwb	:1;
		unsigned int support_ogam	:1;
		unsigned int support_wbscl	:1;
		unsigned int support_ocsc	:1;
	} caps;
	unsigned int	 reserved2[10];	/* Reserved for future use, MUST BE 0. */
};

struct dwbc {
	const struct dwbc_funcs *funcs;
	struct dc_context *ctx;
	int inst;
	struct mcif_wb *mcif;
	bool status;
	int inputSrcSelect;
	bool dwb_output_black;
	enum dc_transfer_func_predefined tf;
	enum dc_color_space output_color_space;
};

struct dwbc_funcs {
	bool (*get_caps)(
		struct dwbc *dwbc,
		struct dwb_caps *caps);

	bool (*enable)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);

	bool (*disable)(struct dwbc *dwbc);

	bool (*update)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);

	bool (*is_enabled)(
		struct dwbc *dwbc);

	void (*set_stereo)(
		struct dwbc *dwbc,
		struct dwb_stereo_params *stereo_params);

	void (*set_new_content)(
		struct dwbc *dwbc,
		bool is_new_content);

#if defined(CONFIG_DRM_AMD_DC_DCN2_0)

	void (*set_warmup)(
		struct dwbc *dwbc,
		struct dwb_warmup_params *warmup_params);

#endif

	void (*dwb_set_scaler)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);
};

#endif
