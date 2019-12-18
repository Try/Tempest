/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#ifdef _WIN32
#ifdef __MINGW32__
#define _WIN32_IE 0x501
#else
#define _WIN32_IE 0x400
#endif
#endif

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "alMain.h"

#ifdef _WIN32_IE
//#include <shlobj.h>
#endif

typedef struct ConfigEntry {
    char *key;
    char *value;
} ConfigEntry;

typedef struct ConfigBlock {
    char *name;
    ConfigEntry *entries;
    unsigned int entryCount;
} ConfigBlock;

static ConfigBlock *cfgBlocks;
static unsigned int cfgCount;

void ReadALConfig(void)
{
    cfgBlocks = calloc(1, sizeof(ConfigBlock));
    cfgBlocks->name = strdup("general");
    cfgCount = 1;
}

void FreeALConfig(void)
{
    unsigned int i;

    for(i = 0;i < cfgCount;i++)
    {
        unsigned int j;
        for(j = 0;j < cfgBlocks[i].entryCount;j++)
        {
           free(cfgBlocks[i].entries[j].key);
           free(cfgBlocks[i].entries[j].value);
        }
        free(cfgBlocks[i].entries);
        free(cfgBlocks[i].name);
    }
    free(cfgBlocks);
    cfgBlocks = NULL;
    cfgCount = 0;
}

const char *GetConfigValue(const char *blockName, const char *keyName, const char *def)
{
    unsigned int i, j;

    if(!keyName)
        return def;

    if(!blockName)
        blockName = "general";

    for(i = 0;i < cfgCount;i++)
    {
        if(strcmp(cfgBlocks[i].name, blockName) != 0)
            continue;

        for(j = 0;j < cfgBlocks[i].entryCount;j++)
        {
            if(strcmp(cfgBlocks[i].entries[j].key, keyName) == 0)
            {
                TRACE("Found %s:%s = \"%s\"\n", blockName, keyName,
                      cfgBlocks[i].entries[j].value);
                if(cfgBlocks[i].entries[j].value[0])
                    return cfgBlocks[i].entries[j].value;
                return def;
            }
        }
    }

    TRACE("Key %s:%s not found\n", blockName, keyName);
    return def;
}

int ConfigValueExists(const char *blockName, const char *keyName)
{
    const char *val = GetConfigValue(blockName, keyName, "");
    return !!val[0];
}

int ConfigValueStr(const char *blockName, const char *keyName, const char **ret)
{
    const char *val = GetConfigValue(blockName, keyName, "");
    if(!val[0]) return 0;

    *ret = val;
    return 1;
}

int ConfigValueInt(const char *blockName, const char *keyName, int *ret)
{
    const char *val = GetConfigValue(blockName, keyName, "");
    if(!val[0]) return 0;

    *ret = strtol(val, NULL, 0);
    return 1;
}

int ConfigValueUInt(const char *blockName, const char *keyName, unsigned int *ret)
{
    const char *val = GetConfigValue(blockName, keyName, "");
    if(!val[0]) return 0;

    *ret = strtoul(val, NULL, 0);
    return 1;
}

int ConfigValueFloat(const char *blockName, const char *keyName, float *ret)
{
    const char *val = GetConfigValue(blockName, keyName, "");
    if(!val[0]) return 0;

#ifdef HAVE_STRTOF
    *ret = strtof(val, NULL);
#else
    *ret = (float)strtod(val, NULL);
#endif
    return 1;
}

int GetConfigValueBool(const char *blockName, const char *keyName, int def)
{
    const char *val = GetConfigValue(blockName, keyName, "");

    if(!val[0]) return !!def;
    return (strcmp(val, "true") == 0 || strcmp(val, "yes") == 0 ||
            strcmp(val, "on") == 0 || atoi(val) != 0);
}
