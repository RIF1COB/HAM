/*
 * structured_logging.c
 */

#include "structured_logging.h"

const char *sh_log_tag_to_str(sh_log_tag_t tag)
{
    switch (tag) {
        case LOG_TAG_CFG: return "CFG";
        case LOG_TAG_CTRL: return "CTRL";
        case LOG_TAG_DRV: return "DRV";
        case LOG_TAG_AWS: return "AWS";
        case LOG_TAG_BLE: return "BLE";
        case LOG_TAG_OTA: return "OTA";
        case LOG_TAG_FACTORY: return "FACT";
        default: return "APP";
    }
}
