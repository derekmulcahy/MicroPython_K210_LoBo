/*
 * This file is part of the MicroPython K210 project, https://github.com/loboris/MicroPython_K210_LoBo
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 LoBo (https://github.com/loboris)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "mpconfigport.h"

#ifdef MICROPY_USE_DISPLAY

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "moddisplay.h"
#include "modmachine.h"

#include "py/runtime.h"

// constructor(id, ...)
//-----------------------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {

    display_tft_obj_t *self = m_new_obj(display_tft_obj_t);
    self->base.type = &display_tft_type;

    return MP_OBJ_FROM_PTR(self);
}

//-----------------------------------------------------------------------------------------------
STATIC void display_tft_printinfo(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    display_tft_obj_t *self = self_in;
    mp_printf(print, "TFT Display");
}

/*
 * Check active display device type
 * Reinitialize SPI, if the spi device changed
 * Set some global variables so that multiple displays can be used
 */
//-------------------------------------------
static int setupDevice( mp_obj_t disp_dev_in)
{
    return 0;
}

//--------------------------------------
STATIC color_t intToColor(uint32_t cint)
{
    color_t cl = {0,0,0};
    cl.r = (cint >> 16) & 0xFF;
    cl.g = (cint >> 8) & 0xFF;
    cl.b = cint & 0xFF;
    return cl;
}

//------------------------------------------------------
STATIC void spi_deinit_internal(display_tft_obj_t *self)
{
}


//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_type, ARG_width, ARG_height, ARG_speed, ARG_bckl, ARG_bcklon, ARG_cbits, ARG_rot, ARG_bgr, ARG_splash };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_type,      MP_ARG_REQUIRED                   | MP_ARG_INT,  { .u_int = DISP_TYPE_ST7789V } },
        { MP_QSTR_width,                       MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = DEFAULT_TFT_DISPLAY_WIDTH } },
        { MP_QSTR_height,                      MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = DEFAULT_TFT_DISPLAY_HEIGHT } },
        { MP_QSTR_speed,                       MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 10000000 } },
        { MP_QSTR_backl_pin,                   MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = -1 } },
        { MP_QSTR_backl_on,                    MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 0 } },
        { MP_QSTR_color_bits,                  MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 16 } },
        { MP_QSTR_rot,                         MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = -1 } },
        { MP_QSTR_bgr,                         MP_ARG_KW_ONLY  | MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_splash,                      MP_ARG_KW_ONLY  | MP_ARG_BOOL, { .u_bool = true } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    display_tft_obj_t *self = pos_args[0];
    int ret;

    if ((args[ARG_type].u_int < 0) || (args[ARG_type].u_int >= DISP_TYPE_MAX)) {
        mp_raise_ValueError("Unsupported display type");
    }

    if ((args[ARG_cbits].u_int != 16) && (args[ARG_cbits].u_int != 24)) {
        mp_raise_ValueError("Unsupported color bits");
    }

    self->dconfig.color_bits = args[ARG_cbits].u_int;

    self->dconfig.type = args[ARG_type].u_int;

    self->dconfig.gamma = 0;
    self->dconfig.width = args[ARG_width].u_int;   // smaller dimension
    self->dconfig.height = args[ARG_height].u_int; // larger dimension
    self->dconfig.invrot = 1;

    self->dconfig.bgr = args[ARG_speed].u_bool ? 0x08 : 0;
    self->dconfig.speed = args[ARG_speed].u_int;

    self->dconfig.bckl = args[ARG_bckl].u_int;
    self->dconfig.bckl_on = args[ARG_bcklon].u_int & 1;


    int orient = LANDSCAPE;
    if ((args[ARG_rot].u_int >= 0) && (args[ARG_rot].u_int <= 3)) orient = args[ARG_rot].u_int;

    // ================================
    // ==== Initialize the Display ====
    ret = TFT_display_init(&self->dconfig);
    if (ret != 0) {
        mp_raise_msg(&mp_type_OSError, "Error initializing display");
    }

    font_rotate = 0;
    text_wrap = 0;
    font_transparent = 0;
    font_forceFixed = 0;
    gray_scale = 0;
    TFT_setRotation(orient);
    self->dconfig.width = _width;
    self->dconfig.height = _height;
    TFT_setGammaCurve(0);
    TFT_setFont(DEFAULT_FONT, NULL, false);
    TFT_resetclipwin();
    if (args[ARG_splash].u_bool) {
        int fhight = TFT_getfontheight();
        _fg = intToColor(iTFT_RED);
        TFT_print("MicroPython", CENTER, (_height/2) - fhight - (fhight/2));
        _fg = intToColor(iTFT_GREEN);
        TFT_print("MicroPython", CENTER, (_height/2) - (fhight/2));
        _fg = intToColor(iTFT_BLUE);
        TFT_print("MicroPython", CENTER, (_height/2) + (fhight/2));
        _fg = intToColor(iTFT_GREEN);
    }

    bcklOn(&self->dconfig);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_init_obj, 0, display_tft_init);

//--------------------------------------------------
STATIC mp_obj_t display_tft_deinit(mp_obj_t self_in)
{
    display_tft_obj_t *self = self_in;
    if (setupDevice(self_in) == 0) {
        spi_deinit_internal(self);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(display_tft_deinit_obj, display_tft_deinit);

//-------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawPixel(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,               MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    if (args[2].u_int >= 0) {
        color = intToColor(args[2].u_int);
    }
    TFT_drawPixel(x, y, color, 1);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawPixel_obj, 2, display_tft_drawPixel);

//------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawLine(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1,    MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1,    MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                   MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
    mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;
    if (args[4].u_int >= 0) {
        color = intToColor(args[4].u_int);
    }
    TFT_drawLine(x0, y0, x1, y1, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawLine_obj, 4, display_tft_drawLine);

//-------------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawLineByAngle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_start,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_length, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_angle,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t start = args[2].u_int;
    mp_int_t len = args[3].u_int;
    mp_int_t angle = args[4].u_int;
    if (args[5].u_int >= 0) {
        color = intToColor(args[5].u_int);
    }
    TFT_drawLineByAngle(x, y, start, len, angle, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawLineByAngle_obj, 5, display_tft_drawLineByAngle);

//----------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawTriangle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
    mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;
    mp_int_t x2 = args[4].u_int;
    mp_int_t y2 = args[5].u_int;
    if (args[6].u_int >= 0) {
        color = intToColor(args[6].u_int);
    }
    if (args[7].u_int >= 0) {
        TFT_fillTriangle(x0, y0, x1, y1, x2, y2, intToColor(args[7].u_int));
    }
    TFT_drawTriangle(x0, y0, x1, y1, x2, y2, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawTriangle_obj, 6, display_tft_drawTriangle);

//--------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawCircle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t radius = args[2].u_int;
    if (args[3].u_int >= 0) {
        color = intToColor(args[3].u_int);
    }
    if (args[4].u_int >= 0) {
        TFT_fillCircle(x, y, radius, intToColor(args[4].u_int));
        if (args[3].u_int != args[4].u_int) TFT_drawCircle(x, y, radius, color);
    }
    else TFT_drawCircle(x, y, radius, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawCircle_obj, 3, display_tft_drawCircle);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawEllipse(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_rx,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_ry,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_opt,                      MP_ARG_INT, { .u_int = 15 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t rx = args[2].u_int;
    mp_int_t ry = args[3].u_int;
    mp_int_t opt = args[4].u_int & 0x0F;
    if (args[5].u_int >= 0) {
        color = intToColor(args[5].u_int);
    }
    if (args[6].u_int >= 0) {
        TFT_fillEllipse(x, y, rx, ry, intToColor(args[6].u_int), opt);
    }
    TFT_drawEllipse(x, y, rx, ry, color, opt);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawEllipse_obj, 4, display_tft_drawEllipse);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawArc(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_thick,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_start,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_end,    MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 15 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    color_t fill_color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t r = args[2].u_int;
    mp_int_t th = args[3].u_int;
    mp_int_t start = args[4].u_int;
    mp_int_t end = args[5].u_int;
    if (args[6].u_int >= 0) {
        color = intToColor(args[6].u_int);
    }
    if (args[7].u_int >= 0) {
        fill_color = intToColor(args[7].u_int);
    }
    TFT_drawArc(x, y, r, th, start, end, color, fill_color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawArc_obj, 6, display_tft_drawArc);

//------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawPoly(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_sides,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_thick,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 1 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_rotate,                   MP_ARG_INT, { .u_int = 0 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    color_t fill_color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t r = args[2].u_int;
    mp_int_t sides = args[3].u_int;
    mp_int_t th = args[4].u_int;
    if (args[5].u_int >= 0) {
        color = intToColor(args[5].u_int);
    }
    if (args[6].u_int >= 0) {
        fill_color = intToColor(args[6].u_int);
    }
    TFT_drawPolygon(x, y, sides, r, color, fill_color, args[7].u_int, th);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawPoly_obj, 5, display_tft_drawPoly);

//------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_width,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    if (args[4].u_int >= 0) {
        color = intToColor(args[4].u_int);
    }
    if (args[5].u_int >= 0) {
        TFT_fillRect(x, y, w, h, intToColor(args[5].u_int));
    }
    TFT_drawRect(x, y, w, h, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawRect_obj, 4, display_tft_drawRect);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_writeScreen(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_width,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_buffer, MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;

    // clipping
    if ((x >= _width) || (y > _height)) {
        mp_raise_ValueError("Point (x,y) outside the display area");
    }

    if (x < 0) {
        w -= (0 - x);
        x = 0;
    }
    if (y < 0) {
        h -= (0 - y);
        y = 0;
    }
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    if ((x + w) > (_width+1)) w = _width - x + 1;
    if ((y + h) > (_height+1)) h = _height - y + 1;
    if (w == 0) w = 1;
    if (h == 0) h = 1;

    int clr_len = h*w;
    int buf_len = (clr_len*3) + 1;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[4].u_obj, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != buf_len) {
        mp_raise_ValueError("Wrong buffer length");
    }

    send_data(x, y, x+w+1, y+h+1, (uint32_t)clr_len, (color_t *)bufinfo.buf + 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_writeScreen_obj, 5, display_tft_writeScreen);

//-----------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_drawRoundRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_width,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _fg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    mp_int_t r = args[4].u_int;
    if (args[5].u_int >= 0) {
        color = intToColor(args[5].u_int);
    }
    if (args[6].u_int >= 0) {
        TFT_fillRoundRect(x, y, w, h, r, intToColor(args[6].u_int));
    }
    TFT_drawRoundRect(x, y, w, h, r, color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_drawRoundRect_obj, 5, display_tft_drawRoundRect);

//--------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_fillScreen(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _bg;
    if (args[0].u_int >= 0) {
        color = intToColor(args[0].u_int);
    }
    TFT_fillScreen(color);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_fillScreen_obj, 0, display_tft_fillScreen);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_fillWin(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t color = _bg;
    if (args[0].u_int >= 0) {
        color = intToColor(args[0].u_int);
    }
    TFT_fillWindow(color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_fillWin_obj, 0, display_tft_fillWin);

//--------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_7segAttrib(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_dist,    MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_width,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_outline, MP_ARG_REQUIRED | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_color,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    set_7seg_font_atrib(args[0].u_int, args[1].u_int, (int)args[2].u_bool, intToColor(args[3].u_int));

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_7segAttrib_obj, 4, display_tft_7segAttrib);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_setFont(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_font,         MP_ARG_REQUIRED | MP_ARG_OBJ,  { .u_obj = mp_const_none } },
        { MP_QSTR_rotate,       MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = -1 } },
        { MP_QSTR_transparent,  MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = -1 } },
        { MP_QSTR_fixedwidth,   MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = -1 } },
        { MP_QSTR_dist,         MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 8 } },
        { MP_QSTR_width,        MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 2 } },
        { MP_QSTR_outline,      MP_ARG_KW_ONLY  | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_color,        MP_ARG_KW_ONLY  | MP_ARG_INT,  { .u_int = 0 } },
        { MP_QSTR_info,         MP_ARG_KW_ONLY  | MP_ARG_BOOL, { .u_bool = false } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t font = DEFAULT_FONT;
    mp_obj_t fontf = NULL;

    if (MP_OBJ_IS_STR(args[0].u_obj)) {
        font = USER_FONT;
        fontf = args[0].u_obj;
    }
    else {
        font = mp_obj_get_int(args[0].u_obj);
    }
    bool res = TFT_setFont(font, fontf, args[8].u_bool);

    if (args[1].u_int >= 0) font_rotate = args[1].u_int;
    if (args[2].u_int >= 0) font_transparent = args[2].u_int & 1;
    if (args[3].u_int >= 0) font_forceFixed = args[3].u_int & 1;

    if (font == FONT_7SEG) {
        set_7seg_font_atrib(args[4].u_int, args[5].u_int, (int)args[6].u_bool, intToColor(args[7].u_int));
    }
    if (res) return mp_const_true;
    return mp_const_false;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_setFont_obj, 1, display_tft_setFont);

//-------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_getFontSize(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    int width, height;
    TFT_getfontsize(&width, &height);

    mp_obj_t tuple[2];

    tuple[0] = mp_obj_new_int(width);
    tuple[1] = mp_obj_new_int(height);

    return mp_obj_new_tuple(2, tuple);
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_getFontSize_obj, 0, display_tft_getFontSize);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_setRot(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_rot, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = PORTRAIT } },
    };
    display_tft_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t rot = args[0].u_int;
    if ((rot < 0) || (rot > 255)) rot = 0;

    TFT_setRotation(rot);
    self->dconfig.width = _width;
    self->dconfig.height = _height;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_setRot_obj, 1, display_tft_setRot);

//---------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_print(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,            MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,            MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_text,         MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_color,                          MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_rotate,       MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_transparent,  MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_fixedwidth,   MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_wrap,         MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_bgcolor,      MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t old_fg = _fg;
    color_t old_bg = _bg;
    int old_rot = font_rotate;
    int old_transp = font_transparent;
    int old_fixed = font_forceFixed;
    int old_wrap = text_wrap;

    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    char *st = (char *)mp_obj_str_get_str(args[2].u_obj);

    if (args[3].u_int >= 0) _fg = intToColor(args[3].u_int);
    if (args[4].u_int >= 0) font_rotate = args[4].u_int;
    if (args[5].u_int >= 0) font_transparent = args[5].u_int & 1;
    if (args[6].u_int >= 0) font_forceFixed = args[6].u_int & 1;
    if (args[7].u_int >= 0) text_wrap = args[7].u_int & 1;
    if (args[8].u_int >= 0) _bg = intToColor(args[8].u_int);

    TFT_print(st, x, y);

    _fg = old_fg;
    _bg = old_bg;
    font_rotate = old_rot;
    font_transparent = old_transp;
    font_forceFixed = old_fixed;
    text_wrap = old_wrap;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_print_obj, 3, display_tft_print);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_stringWidth(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_text,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    char *st = (char *)mp_obj_str_get_str(args[0].u_obj);

    mp_int_t w = TFT_getStringWidth(st);

    return mp_obj_new_int(w);
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_stringWidth_obj, 1, display_tft_stringWidth);

//-------------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_clearStringRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,       MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,       MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_text,    MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_color,                     MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    color_t old_bg = _bg;
    mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    char *st = (char *)mp_obj_str_get_str(args[2].u_obj);

    if (args[3].u_int >= 0) _bg = intToColor(args[3].u_int);

    TFT_clearStringRect(x, y, st);

    _bg = old_bg;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_clearStringRect_obj, 3, display_tft_clearStringRect);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_Image(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_file,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_scale,                   MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_type,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_debug, MP_ARG_KW_ONLY  | MP_ARG_INT, { .u_int = 0 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (!MP_OBJ_IS_STR(args[2].u_obj)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "file name expected"));
    }
    int img_type = args[4].u_int;

    if (img_type < 0) {
        // try to determine image type
        char *fname = (char *)mp_obj_str_get_str(args[2].u_obj);
        char upr_fname[strlen(fname)+1];
        strcpy(upr_fname, fname);
        for (int i=0; i < strlen(upr_fname); i++) {
          upr_fname[i] = toupper((unsigned char) upr_fname[i]);
        }
        if (strstr(upr_fname, ".JPG") != NULL) img_type = IMAGE_TYPE_JPG;
        else if (strstr(upr_fname, ".BMP") != NULL) img_type = IMAGE_TYPE_BMP;
        else {
            // Open the file
            mp_obj_t fargs[2];
            fargs[0] = args[2].u_obj;
            fargs[1] = mp_obj_new_str("rb", 2);
            mp_obj_t ffd = mp_vfs_open(2, args, (mp_map_t*)&mp_const_empty_map);
            if (ffd) {
                uint8_t buf[16];
                if ( mp_stream_posix_read(ffd, buf, 11) == 11) {
                    buf[10] = 0;
                    if (strstr((char *)(buf+6), "JFIF") != NULL) img_type = IMAGE_TYPE_JPG;
                    else if ((buf[0] = 0x42) && (buf[1] = 0x4d)) img_type = IMAGE_TYPE_BMP;
                }
                mp_stream_close(ffd);
            }
        }
        if (img_type < 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Cannot determine image type"));
        }
    }

    image_debug = (uint8_t)args[5].u_bool;
    if (img_type == IMAGE_TYPE_BMP) {
        if (tft_active_mode == TFT_MODE_EPD) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "BMP not supported on EPD"));
        }
        TFT_bmp_image(args[0].u_int, args[1].u_int, args[3].u_int, args[2].u_obj, NULL, 0);
    }
    else if (img_type == IMAGE_TYPE_JPG) {
        TFT_jpg_image(args[0].u_int, args[1].u_int, args[3].u_int, args[2].u_obj, NULL, 0);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Unsupported image type"));
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_Image_obj, 3, display_tft_Image);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_compileFont(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_file,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_debug, MP_ARG_KW_ONLY  | MP_ARG_BOOL, { .u_bool = false } },
    };
    //display_tft_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    //if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    //res = compile_font_file(fullname, debug);
    //if (res) return mp_const_false;
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_compileFont_obj, 1, display_tft_compileFont);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_HSBtoRGB(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_hue,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_saturation,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_brightness,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_float_t hue = mp_obj_get_float(args[0].u_obj);
    mp_float_t sat = mp_obj_get_float(args[1].u_obj);
    mp_float_t bri = mp_obj_get_float(args[2].u_obj);

    color_t color = HSBtoRGB(hue, sat, bri);
    mp_int_t icolor = (int)((color.r << 16) | (color.g << 8) | color.b);

    return mp_obj_new_int(icolor);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_HSBtoRGB_obj, 3, display_tft_HSBtoRGB);

//--------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_setclipwin(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,  MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_fillcolor,                MP_ARG_INT, { .u_int = -1 } },
    };
    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
    mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;

    TFT_setclipwin(x0, y0, x1, y1);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_setclipwin_obj, 4, display_tft_setclipwin);

//----------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_resetclipwin(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    TFT_resetclipwin();

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_resetclipwin_obj, 0, display_tft_resetclipwin);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_saveclipwin(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    TFT_saveClipWin();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_saveclipwin_obj, 0, display_tft_saveclipwin);

//------------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_restoreclipwin(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    TFT_restoreClipWin();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_restoreclipwin_obj, 0, display_tft_restoreclipwin);

//------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_getSize(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(_width);
    tuple[1] = mp_obj_new_int(_height);

    return mp_obj_new_tuple(2, tuple);
}
MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_getSize_obj, 0, display_tft_getSize);

//--------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_tft_getWinSize(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (setupDevice(MP_OBJ_TO_PTR(pos_args[0]))) return mp_const_none;

    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(dispWin.x2 - dispWin.x1 + 1);
    tuple[1] = mp_obj_new_int(dispWin.y2 - dispWin.y1 + 1);

    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_tft_getWinSize_obj, 0, display_tft_getWinSize);

//------------------------------------------------------------------------
STATIC mp_obj_t display_tft_backlight(mp_obj_t self_in, mp_obj_t onoff_in)
{
    display_tft_obj_t *self = self_in;
    if (setupDevice(self_in)) return mp_const_none;

    int onoff = mp_obj_get_int(onoff_in);
    if (onoff) bcklOn(&self->dconfig);
    else bcklOff(&self->dconfig);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(display_tft_backlight_obj, display_tft_backlight);

//--------------------------------------------------
STATIC mp_obj_t display_tft_get_bg(mp_obj_t self_in)
{
    int icolor = (int)((_bg.r << 16) | (_bg.g << 8) | _bg.b);
    return mp_obj_new_int(icolor);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_tft_get_bg_obj, display_tft_get_bg);

//--------------------------------------------------
STATIC mp_obj_t display_tft_get_fg(mp_obj_t self_in)
{
    int icolor = (int)((_fg.r << 16) | (_fg.g << 8) | _fg.b);
    return mp_obj_new_int(icolor);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_tft_get_fg_obj, display_tft_get_fg);

//---------------------------------------------------------------------
STATIC mp_obj_t display_tft_set_bg(mp_obj_t self_in, mp_obj_t color_in)
{
    color_t color = intToColor(mp_obj_get_int(color_in));
    _bg = color;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(display_tft_set_bg_obj, display_tft_set_bg);

//---------------------------------------------------------------------
STATIC mp_obj_t display_tft_set_fg(mp_obj_t self_in, mp_obj_t color_in)
{
    color_t color = intToColor(mp_obj_get_int(color_in));
    _fg = color;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(display_tft_set_fg_obj, display_tft_set_fg);

//-------------------------------------------------
STATIC mp_obj_t display_tft_get_X(mp_obj_t self_in)
{
    int x = TFT_X;
    return mp_obj_new_int(x);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_tft_get_X_obj, display_tft_get_X);

//-------------------------------------------------
STATIC mp_obj_t display_tft_get_Y(mp_obj_t self_in)
{
    int y = TFT_Y;
    return mp_obj_new_int(y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_tft_get_Y_obj, display_tft_get_Y);

//------------------------------------------------
STATIC mp_obj_t display_tft_test(mp_obj_t self_in)
{
    uint16_t dx = 50, dy = 20, cx, cy;
    int len = dx * dy;
    int i, j, bufpos;
    color_t color = {255, 255, 255};
    color_t *color_line = malloc(len*3);
    for (int n = 0; n < len; n++) {
        color_line[n] = (color_t){0, 0, 0};
    }
    for (j=0; j < dy; j++) {
        for (i=0; i < dx; i++) {
            if (1) {
                // visible pixel
                cx = (uint16_t)(i);
                cy = (uint16_t)(j);
                bufpos = (dx * cy) + cx;
                color_line[bufpos] = color;
            }
        }
    }
    send_data(0, 0, dx, dy, len, color_line);

    for (int n = 0; n < len; n++) {
        color_line[n] = (color_t){0, 0, 0};
    }
    for (j=0; j < dy; j++) {
        for (i=0; i < dx; i++) {
            if ((i % 5) == 0) {
                // visible pixel
                cx = (uint16_t)(i);
                cy = (uint16_t)(j);
                bufpos = (dx * cy) + cx;
                color_line[bufpos] = color;
            }
        }
    }
    send_data(110, 0, 110+dx, dy, len, color_line);

    for (int n = 0; n < len; n++) {
        color_line[n] = (color_t){0, 0, 0};
    }
    for (j=0; j < dy; j++) {
        for (i=0; i < dx; i++) {
            if ((j % 4) == 0) {
                // visible pixel
                cx = (uint16_t)(i);
                cy = (uint16_t)(j);
                bufpos = (dx * cy) + cx;
                color_line[bufpos] = color;
            }
        }
    }
    send_data(0, 110, dx, 110+dy, len, color_line);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_tft_test_obj, display_tft_test);


//================================================================
STATIC const mp_rom_map_elem_t display_tft_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_test),                MP_ROM_PTR(&display_tft_test_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),                MP_ROM_PTR(&display_tft_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),              MP_ROM_PTR(&display_tft_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel),               MP_ROM_PTR(&display_tft_drawPixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_line),                MP_ROM_PTR(&display_tft_drawLine_obj) },
    { MP_ROM_QSTR(MP_QSTR_lineByAngle),         MP_ROM_PTR(&display_tft_drawLineByAngle_obj) },
    { MP_ROM_QSTR(MP_QSTR_triangle),            MP_ROM_PTR(&display_tft_drawTriangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_circle),              MP_ROM_PTR(&display_tft_drawCircle_obj) },
    { MP_ROM_QSTR(MP_QSTR_ellipse),             MP_ROM_PTR(&display_tft_drawEllipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_arc),                 MP_ROM_PTR(&display_tft_drawArc_obj) },
    { MP_ROM_QSTR(MP_QSTR_polygon),             MP_ROM_PTR(&display_tft_drawPoly_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect),                MP_ROM_PTR(&display_tft_drawRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeScreen),         MP_ROM_PTR(&display_tft_writeScreen_obj) },
    { MP_ROM_QSTR(MP_QSTR_roundrect),           MP_ROM_PTR(&display_tft_drawRoundRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear),               MP_ROM_PTR(&display_tft_fillScreen_obj) },
    { MP_ROM_QSTR(MP_QSTR_clearwin),            MP_ROM_PTR(&display_tft_fillWin_obj) },
    { MP_ROM_QSTR(MP_QSTR_font),                MP_ROM_PTR(&display_tft_setFont_obj) },
    { MP_ROM_QSTR(MP_QSTR_fontSize),            MP_ROM_PTR(&display_tft_getFontSize_obj) },
    { MP_ROM_QSTR(MP_QSTR_text),                MP_ROM_PTR(&display_tft_print_obj) },
    { MP_ROM_QSTR(MP_QSTR_orient),              MP_ROM_PTR(&display_tft_setRot_obj) },
    { MP_ROM_QSTR(MP_QSTR_textWidth),           MP_ROM_PTR(&display_tft_stringWidth_obj) },
    { MP_ROM_QSTR(MP_QSTR_textClear),           MP_ROM_PTR(&display_tft_clearStringRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_attrib7seg),          MP_ROM_PTR(&display_tft_7segAttrib_obj) },
    { MP_ROM_QSTR(MP_QSTR_image),               MP_ROM_PTR(&display_tft_Image_obj) },
    { MP_ROM_QSTR(MP_QSTR_compileFont),         MP_ROM_PTR(&display_tft_compileFont_obj) },
    { MP_ROM_QSTR(MP_QSTR_hsb2rgb),             MP_ROM_PTR(&display_tft_HSBtoRGB_obj) },
    { MP_ROM_QSTR(MP_QSTR_setwin),              MP_ROM_PTR(&display_tft_setclipwin_obj) },
    { MP_ROM_QSTR(MP_QSTR_resetwin),            MP_ROM_PTR(&display_tft_resetclipwin_obj) },
    { MP_ROM_QSTR(MP_QSTR_savewin),             MP_ROM_PTR(&display_tft_saveclipwin_obj) },
    { MP_ROM_QSTR(MP_QSTR_restorewin),          MP_ROM_PTR(&display_tft_restoreclipwin_obj) },
    { MP_ROM_QSTR(MP_QSTR_screensize),          MP_ROM_PTR(&display_tft_getSize_obj) },
    { MP_ROM_QSTR(MP_QSTR_winsize),             MP_ROM_PTR(&display_tft_getWinSize_obj) },
    { MP_ROM_QSTR(MP_QSTR_backlight),           MP_ROM_PTR(&display_tft_backlight_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_fg),              MP_ROM_PTR(&display_tft_get_fg_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_bg),              MP_ROM_PTR(&display_tft_get_bg_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_fg),              MP_ROM_PTR(&display_tft_set_fg_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_bg),              MP_ROM_PTR(&display_tft_set_bg_obj) },
    { MP_ROM_QSTR(MP_QSTR_text_x),              MP_ROM_PTR(&display_tft_get_X_obj) },
    { MP_ROM_QSTR(MP_QSTR_text_y),              MP_ROM_PTR(&display_tft_get_Y_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_CENTER),              MP_ROM_INT(CENTER) },
    { MP_ROM_QSTR(MP_QSTR_RIGHT),               MP_ROM_INT(RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_BOTTOM),              MP_ROM_INT(BOTTOM) },
    { MP_ROM_QSTR(MP_QSTR_LASTX),               MP_ROM_INT(LASTX) },
    { MP_ROM_QSTR(MP_QSTR_LASTY),               MP_ROM_INT(LASTY) },

    { MP_ROM_QSTR(MP_QSTR_PORTRAIT),            MP_ROM_INT(PORTRAIT) },
    { MP_ROM_QSTR(MP_QSTR_LANDSCAPE),           MP_ROM_INT(LANDSCAPE) },
    { MP_ROM_QSTR(MP_QSTR_PORTRAIT_FLIP),       MP_ROM_INT(PORTRAIT_FLIP) },
    { MP_ROM_QSTR(MP_QSTR_LANDSCAPE_FLIP),      MP_ROM_INT(LANDSCAPE_FLIP) },

    { MP_ROM_QSTR(MP_QSTR_FONT_Default),        MP_ROM_INT(DEFAULT_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_DejaVu18),       MP_ROM_INT(DEJAVU18_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_DejaVu24),       MP_ROM_INT(DEJAVU24_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_Ubuntu),         MP_ROM_INT(UBUNTU16_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_Comic),          MP_ROM_INT(COMIC24_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_Minya),          MP_ROM_INT(MINYA24_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_Tooney),         MP_ROM_INT(TOONEY32_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_Small),          MP_ROM_INT(SMALL_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_DefaultSmall),   MP_ROM_INT(DEF_SMALL_FONT) },
    { MP_ROM_QSTR(MP_QSTR_FONT_7seg),           MP_ROM_INT(FONT_7SEG) },
    { MP_ROM_QSTR(MP_QSTR_FONT_USER1),          MP_ROM_INT(USER_FONT_1) },
    { MP_ROM_QSTR(MP_QSTR_FONT_USER2),          MP_ROM_INT(USER_FONT_2) },
    { MP_ROM_QSTR(MP_QSTR_FONT_USER3),          MP_ROM_INT(USER_FONT_3) },
    { MP_ROM_QSTR(MP_QSTR_FONT_USER4),          MP_ROM_INT(USER_FONT_4) },

    { MP_ROM_QSTR(MP_QSTR_BLACK),               MP_ROM_INT(iTFT_BLACK) },
    { MP_ROM_QSTR(MP_QSTR_NAVY),                MP_ROM_INT(iTFT_NAVY) },
    { MP_ROM_QSTR(MP_QSTR_DARKGREEN),           MP_ROM_INT(iTFT_DARKGREEN) },
    { MP_ROM_QSTR(MP_QSTR_DARKCYAN),            MP_ROM_INT(iTFT_DARKCYAN) },
    { MP_ROM_QSTR(MP_QSTR_MAROON),              MP_ROM_INT(iTFT_MAROON) },
    { MP_ROM_QSTR(MP_QSTR_PURPLE),              MP_ROM_INT(iTFT_PURPLE) },
    { MP_ROM_QSTR(MP_QSTR_OLIVE),               MP_ROM_INT(iTFT_OLIVE) },
    { MP_ROM_QSTR(MP_QSTR_LIGHTGREY),           MP_ROM_INT(iTFT_LIGHTGREY) },
    { MP_ROM_QSTR(MP_QSTR_DARKGREY),            MP_ROM_INT(iTFT_DARKGREY) },
    { MP_ROM_QSTR(MP_QSTR_BLUE),                MP_ROM_INT(iTFT_BLUE) },
    { MP_ROM_QSTR(MP_QSTR_GREEN),               MP_ROM_INT(iTFT_GREEN) },
    { MP_ROM_QSTR(MP_QSTR_CYAN),                MP_ROM_INT(iTFT_CYAN) },
    { MP_ROM_QSTR(MP_QSTR_RED),                 MP_ROM_INT(iTFT_RED) },
    { MP_ROM_QSTR(MP_QSTR_MAGENTA),             MP_ROM_INT(iTFT_MAGENTA) },
    { MP_ROM_QSTR(MP_QSTR_YELLOW),              MP_ROM_INT(iTFT_YELLOW) },
    { MP_ROM_QSTR(MP_QSTR_WHITE),               MP_ROM_INT(iTFT_WHITE) },
    { MP_ROM_QSTR(MP_QSTR_ORANGE),              MP_ROM_INT(iTFT_ORANGE) },
    { MP_ROM_QSTR(MP_QSTR_GREENYELLOW),         MP_ROM_INT(iTFT_GREENYELLOW) },
    { MP_ROM_QSTR(MP_QSTR_PINK),                MP_ROM_INT(iTFT_PINK) },

    { MP_ROM_QSTR(MP_QSTR_COLOR_BITS16),        MP_ROM_INT(16) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_BITS24),        MP_ROM_INT(24) },

    { MP_ROM_QSTR(MP_QSTR_JPG),                 MP_ROM_INT(IMAGE_TYPE_JPG) },
    { MP_ROM_QSTR(MP_QSTR_BMP),                 MP_ROM_INT(IMAGE_TYPE_BMP) },
};
STATIC MP_DEFINE_CONST_DICT(display_tft_locals_dict, display_tft_locals_dict_table);

//======================================
const mp_obj_type_t display_tft_type = {
    { &mp_type_type },
    .name = MP_QSTR_TFT,
    .print = display_tft_printinfo,
    .make_new = display_tft_make_new,
    .locals_dict = (mp_obj_t)&display_tft_locals_dict,
};

#endif // CONFIG_MICROPY_USE_TFT

