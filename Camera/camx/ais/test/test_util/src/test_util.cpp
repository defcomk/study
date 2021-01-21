/* ===========================================================================
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#include "test_util.h"
#include "CameraOSServices.h"

#ifndef __INTEGRITY /* Using expat on Integrity for parsing XML*/
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#else
#include <string.h>
#include "expat.h"
#include "camera_config_dump.h"
#include "camera_config_cvbs_dump.h"
#endif

#ifndef __INTEGRITY
#define XML_GET_INT_ATTR(_var_, _node_, _attr_, _opt_, _type_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        _var_ = (_type_)atoi((const char *)_p_); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        _var_ = (_type_)_default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)

#define XML_GET_FLOAT_ATTR(_var_, _node_, _attr_, _opt_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        _var_ = atof((const char *)_p_); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        _var_ = _default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)

#define XML_GET_STRING_ATTR(_var_, _node_, _attr_, _opt_, _default_) \
do { \
    xmlChar* _p_ = xmlGetProp(_node_, (const xmlChar *)_attr_); \
    if (_p_) { \
        snprintf(_var_, sizeof(_var_), "%s", _p_); \
        xmlFree(_p_); \
    } else if (_opt_) {\
        snprintf(_var_, sizeof(_var_), "%s", _default_); \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _attr_); \
        return -1; \
    } \
} while(0)
#else
#define XML_GET_INT_ATTR(_var_, _attr_, _label_, _opt_, type, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        _var_ = (type)atoi((const char *)_attr_[_p_ + 1]); \
    } else if (_opt_) {\
        _var_ = (type)_default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}

#define XML_GET_FLOAT_ATTR(_var_,_attr_, _label_, _opt_, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        _var_ = atof((const char *)_attr_[_p_ + 1]); \
    } else if (_opt_) {\
        _var_ = _default_; \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}

#define XML_GET_STRING_ATTR(_var_, _attr_, _label_, _opt_, _default_) \
{ \
    int _p_ = xml_find_attr(_label_, _attr_); \
    if (_p_ != -1) { \
        snprintf(_var_, sizeof(_var_), "%s", _attr_[_p_ + 1]); \
    } else if (_opt_) {\
        snprintf(_var_, sizeof(_var_), "%s", _default_); \
    } else  { \
        QCARCAM_ERRORMSG("could not get attribute " _label_); \
    } \
}
#endif

static test_util_color_fmt_t translate_format_string(char* format)
{
    if (!strncmp(format, "rgba8888", strlen("rgba8888")))
    {
        return TESTUTIL_FMT_RGBX_8888;
    }
    else if (!strncmp(format, "rgb888", strlen("rgb888")))
    {
        return TESTUTIL_FMT_RGB_888;
    }
    else if (!strncmp(format, "rgb565", strlen("rgb565")))
    {
        return TESTUTIL_FMT_RGB_565;
    }
    else if (!strncmp(format, "uyvy", strlen("uyvy")))
    {
        return TESTUTIL_FMT_UYVY_8;
    }
    else
    {
        QCARCAM_ERRORMSG("Default to RGB565");
        return TESTUTIL_FMT_RGB_565;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_di_sw_weave_30fps
///
/// @brief Deinterlace 2 fields from souce buf into dest new frame with software weave 30fps method
///
/// @param di_info         Input souce/dest buffer context
/// @param test_util_get_buf_ptr_func    Helper func to get source/destination buffer virtual address
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_di_sw_weave_30fps(test_util_sw_di_t *di_info, test_util_get_buf_ptr_func buf_ptr_func)
{
    unsigned char *odd_field_ptr = NULL;
    unsigned char *even_field_ptr = NULL;
    unsigned char *source_ptr = NULL;
    unsigned char *dest_ptr = NULL;
    test_util_buf_ptr_t buf_ptr;
    uint32_t line;

    buf_ptr_func(di_info, &buf_ptr, &line);

    dest_ptr = buf_ptr.dest;
    source_ptr = buf_ptr.source;

    if (di_info->field_type == QCARCAM_FIELD_ODD_EVEN)
    {
        odd_field_ptr = source_ptr + line * 13;
        even_field_ptr = source_ptr + line * 267;
    }
    else if (di_info->field_type == QCARCAM_FIELD_EVEN_ODD)
    {
        even_field_ptr = source_ptr + line * 14;
        odd_field_ptr = source_ptr + line * 267;
    }
    else
    {
        /* shouldn't display this frame, so do nothing */
        return;
    }

    for(int i = 0; i < (DE_INTERLACE_HEIGHT/2); i++)
    {
        memcpy(dest_ptr, odd_field_ptr, line);
        dest_ptr += line;
        odd_field_ptr += line;
        memcpy(dest_ptr, even_field_ptr, line);
        dest_ptr += line;
        even_field_ptr += line;
    }
}

#ifndef __INTEGRITY
static int parse_input_element (xmlDocPtr doc, xmlNodePtr parent, test_util_xml_input_t* input)
{
    xmlNodePtr cur = parent->xmlChildrenNode;

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"properties"))) {
            XML_GET_INT_ATTR(input->properties.qcarcam_input_id, cur, "input_id", 0, qcarcam_input_desc_t, 0);
            XML_GET_INT_ATTR(input->properties.use_event_callback, cur, "event_callback", 1, int, 1);
            XML_GET_INT_ATTR(input->properties.frame_timeout, cur, "frame_timeout", 1, int, -1);
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"display_setting")))
        {
            char format[16];

            XML_GET_INT_ATTR(input->window_params.display_id, cur, "display_id", 0, int, 0);
            XML_GET_FLOAT_ATTR(input->window_params.window_size[0], cur, "window_width", 1, 1.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_size[1], cur, "window_height", 1, 1.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_pos[0], cur, "window_pos_x", 1, 0.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_pos[1], cur, "window_pos_y", 1, 0.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_source_size[0], cur, "src_width", 1, 1.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_source_size[1], cur, "src_height", 1, 1.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[0], cur, "src_x", 1, 0.0f);
            XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[1], cur, "src_y", 1, 0.0f);
            XML_GET_INT_ATTR(input->window_params.zorder, cur, "zorder", 1, int, 0);
            XML_GET_INT_ATTR(input->window_params.pipeline_id, cur, "pipeline", 1, int, -1);
            XML_GET_INT_ATTR(input->window_params.n_buffers_display, cur, "nbufs", 1, int, 3);
            XML_GET_STRING_ATTR(format, cur, "format", 1, "uyvy");

            input->window_params.format = translate_format_string(format);
        }
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"output_setting")))
        {
            XML_GET_INT_ATTR(input->output_params.n_buffers, cur, "nbufs", 1, int, 3);
            XML_GET_INT_ATTR(input->output_params.buffer_size[0], cur, "width", 1, int, -1);
            XML_GET_INT_ATTR(input->output_params.buffer_size[1], cur, "height", 1, int, -1);
            XML_GET_INT_ATTR(input->output_params.stride, cur, "stride", 1, int, -1);
            XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_mode, cur, "framedrop_mode", 1, qcarcam_frame_drop_mode_t, 0);
            XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_period, cur, "framedrop_period", 1, char, 0);
            XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_pattern, cur, "framedrop_pattern", 1, int, 0);
        }
#ifdef ENABLE_INJECTION_SUPPORT
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"inject_setting")))
        {
            XML_GET_INT_ATTR(input->inject_params.framedrop_pattern, cur, "framedrop_pattern", 0, qcarcam_color_pattern_t, 0);
            XML_GET_INT_ATTR(input->inject_params.bitdepth, cur, "bitdepth", 0, qcarcam_color_bitdepth_t, 0);
            XML_GET_INT_ATTR(input->inject_params.pack, cur, "pack", 0, qcarcam_color_pack_t, 0);
            XML_GET_INT_ATTR(input->inject_params.buffer_size[0], cur, "width", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.buffer_size[1], cur, "height", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.stride, cur, "stride", 0, int, 0);
            XML_GET_INT_ATTR(input->inject_params.n_buffers, cur, "nbufs", 1, int, 1);
            XML_GET_STRING_ATTR(input->inject_params.filename, cur, "filename", 1, "bayer_input.raw");
        }
#endif
        else if ((!xmlStrcmp(cur->name, (const xmlChar *)"exposure_setting")))
        {
            XML_GET_FLOAT_ATTR(input->exp_params.exposure_time, cur, "exp_time", 1, 31.147f);
            XML_GET_FLOAT_ATTR(input->exp_params.gain, cur, "gain", 1, 1.5f);
            XML_GET_INT_ATTR(input->exp_params.manual_exp, cur, "enable_manual", 1, int, 0);
        }
        cur = cur->next;
    }

    return 0;
}

int test_util_parse_xml_config_file(const char* filename, test_util_xml_input_t* xml_inputs, unsigned int max_num_inputs)
{
    int rc = 0, numInputs = 0;

    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile(filename);

    if (doc == NULL)
    {
        QCARCAM_ERRORMSG("Document not parsed successfully");
        return -1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL)
    {
        QCARCAM_ERRORMSG("Empty config file");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "qcarcam_inputs"))
    {
        QCARCAM_ERRORMSG("Wrong config file format, root node != qcarcam_inputs");
        xmlFreeDoc(doc);
        return -1;
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"input_device")))
        {
            rc = parse_input_element(doc, cur, &xml_inputs[numInputs]);
            if (!rc)
            {
                numInputs++;
            }
            else
            {
                QCARCAM_ERRORMSG("Missing parameter in config file");
            }
        }
        cur = cur->next;
    }

    xmlFreeDoc(doc);
    return numInputs;
}
#else // #ifndef __INTEGRITY

#define MAX_ATTR 30                // Maximum number of attributes for each element in XML
#define MAX_READ_BUFF_SIZE 10000   // Maximum buffer size for file read in bytes

struct xml_data
{
    int root_found;
    int numInputs;
    XML_Parser parser;
    test_util_xml_input_t* inputs;
};

int xml_find_attr(const char *label, const char **attr)
{
    int rc = -1;
    for (int x = 0; x < MAX_ATTR; x += 2)
    {
        if (attr[x] == NULL)
        {
            break;
        }
        else if(!strncmp(label, attr[x], strlen(attr[x])))
        {
            rc = x;
            break;
        }
    }
    return rc;
}

static void xml_read_element(const char *name, const char **attr, test_util_xml_input_t* input)
{
    if (!strncmp(name, "properties", strlen("properties")))
    {
        XML_GET_INT_ATTR(input->properties.qcarcam_input_id, attr, "input_id", 0, qcarcam_input_desc_t, 0);
        XML_GET_INT_ATTR(input->properties.use_event_callback, attr, "event_callback", 1, int, 1);
        XML_GET_INT_ATTR(input->properties.frame_timeout, attr, "frame_timeout", 1, int, -1);

    }
    else if (!strncmp(name, "display_setting", strlen("display_setting")))
    {
        char format[16];

        XML_GET_INT_ATTR(input->window_params.display_id, attr, "display_id", 0, int, 0);
        XML_GET_INT_ATTR(input->window_params.n_buffers_display, attr, "nbufs", 1, int, 3);
        XML_GET_FLOAT_ATTR(input->window_params.window_size[0], attr, "window_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_size[1], attr, "window_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_pos[0], attr, "window_pos_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_pos[1], attr, "window_pos_y", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_size[0], attr, "src_width", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_size[1], attr, "src_height", 1, 1.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[0], attr, "src_x", 1, 0.0f);
        XML_GET_FLOAT_ATTR(input->window_params.window_source_pos[1], attr, "src_y", 1, 0.0f);
        XML_GET_INT_ATTR(input->window_params.zorder, attr, "zorder", 1, int, 0);
        XML_GET_INT_ATTR(input->window_params.pipeline_id, attr, "pipeline", 1, int, -1);
        XML_GET_STRING_ATTR(format, attr, "format", 1, "uyvy");

        input->window_params.format = translate_format_string(format);
    }
    else if (!strncmp(name, "output_setting", strlen("output_setting")))
    {
        XML_GET_INT_ATTR(input->output_params.n_buffers, attr, "nbufs", 1, int, 5);
        XML_GET_INT_ATTR(input->output_params.buffer_size[0], attr, "width", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.buffer_size[1], attr, "height", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.stride, attr, "stride", 1, int, -1);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_mode, attr, "framedrop_mode", 1, qcarcam_frame_drop_mode_t, 0);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_period, attr, "framedrop_period", 1, char, 0);
        XML_GET_INT_ATTR(input->output_params.frame_rate_config.frame_drop_pattern, attr, "framedrop_pattern", 1, int, 0);
    }
#ifdef ENABLE_INJECTION_SUPPORT
    else if (!strncmp(name, "inject_setting", strlen("inject_setting")))
    {
        XML_GET_INT_ATTR(input->inject_params.framedrop_pattern, attr, "framedrop_pattern", 0, qcarcam_color_pattern_t, 0);
        XML_GET_INT_ATTR(input->inject_params.bitdepth, attr, "bitdepth", 0, qcarcam_color_bitdepth_t, 0);
        XML_GET_INT_ATTR(input->inject_params.pack, attr, "pack", 0, qcarcam_color_pack_t, 0);
        XML_GET_INT_ATTR(input->inject_params.buffer_size[0], attr, "width", 0, int, 0);
        XML_GET_INT_ATTR(input->inject_params.buffer_size[1], attr, "height", 1, int, 0);
        XML_GET_INT_ATTR(input->inject_params.stride, attr, "stride", 0, int, 0);
        XML_GET_INT_ATTR(input->inject_params.n_buffers, attr, "nbufs", 1, int, 1);
        XML_GET_STRING_ATTR(input->inject_params.filename, attr, "filename", 1, "bayer_input.raw");
    }
#endif
    else if (!strncmp(name, "exposure_setting", strlen("exposure_setting")))
    {
        XML_GET_FLOAT_ATTR(input->exp_params.exposure_time, attr, "exp_time", 1, 31.147f);
        XML_GET_FLOAT_ATTR(input->exp_params.gain, attr, "gain", 1, 1.5f);
        XML_GET_INT_ATTR(input->exp_params.manual_exp, attr, "enable_manual", 1, int, 0);
    }
}

static void open_element(void *data, const char *name, const char **attr)
{
    xml_data *p_data = (xml_data *)data;

    if (!strncmp(name, "qcarcam_inputs", strlen("qcarcam_inputs")))
    {
        p_data->root_found = 1;
    }

    if (p_data->root_found)
    {
        xml_read_element(name, attr, &p_data->inputs[p_data->numInputs]);
    }
    else
    {
        QCARCAM_ERRORMSG("Document not parsed successfully");
        XML_StopParser(p_data->parser, 0);
    }
}

static void close_element(void *data, const char *name)
{
    xml_data *p_data = (xml_data *)data;

    if (!strncmp(name, "input_device", strlen("input_device")))
    {
        p_data->numInputs++;
    }
}

int test_util_parse_xml_config_file(const char* filename, test_util_xml_input_t* xml_inputs, unsigned int max_num_inputs)
{
    FILE *fp = NULL;
    xml_data data = {};
    char read_buffer[MAX_READ_BUFF_SIZE] = {0};
    size_t length = 0;

    data.inputs = xml_inputs;

    data.parser = XML_ParserCreate(NULL);
    XML_SetUserData(data.parser, &data);
    XML_SetElementHandler(data.parser, open_element, close_element);

    fp = fopen(filename, "rb");

    if (fp != NULL)
    {
        QCARCAM_INFOMSG("Reading config file : ​%s", filename);

        length = fread(&read_buffer, sizeof(char), MAX_READ_BUFF_SIZE, fp);

        if (!XML_Parse(data.parser, read_buffer, length, 1))
        {
            QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
            return 0;
        }

        fclose(fp);
    }
    else
    {
        if (!strncmp(filename, "/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml",
                     strlen("/ghs_test/Camera/config/qcarcam_config_single_cvbs.xml")))
        {
            length = static_cast<size_t>(sizeof(xml_dump_cvbs)/sizeof(xml_dump_cvbs[0]));
            if (!XML_Parse(data.parser, xml_dump_cvbs, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
        else
        {
            length = static_cast<size_t>(sizeof(xml_dump)/sizeof(xml_dump[0]));
            if (!XML_Parse(data.parser, xml_dump, length, 1))
            {
                QCARCAM_ERRORMSG("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(data.parser)), XML_GetCurrentLineNumber(data.parser));
                return 0;
            }
        }
    }

    XML_ParserFree(data.parser);
    return data.numInputs;
}
#endif

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_time
///
/// @brief Get current time
///
/// @param pTime          Pointer to current time
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
int test_util_get_time(unsigned long *pTime)
{
    uint64 tm;
    CameraGetTime(&tm);
    if( pTime ){
        *pTime = tm/1000000;
    }
    return 0;
}

