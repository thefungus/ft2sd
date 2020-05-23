#include <coreinit/time.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <limits.h>
#include "fs/fs_utils.h"
#include "main.h"
#include "console.h"

#define BUFFER_SIZE     0x80000

static const char* game_ids[] =
{
	"10144f00",
	"10145000",
	"10110e00"
};

static int DumpFile(char *pPath, const char * output_path)
{
	char *pPathShort = strstr(pPath, "0005000");
	if (pPathShort != NULL) 
	{
		pPathShort += 18;
	}
	else 
	{
		pPathShort = strchr(pPath, '/');
		if (pPathShort && pPathShort[1] == '/')
			pPathShort++;
	}

	struct stat buffer;
	if (stat(output_path, &buffer) == 0) {
		console_printf(1, "Skipping %s\n", pPathShort);
		return 0;
	}

    char *pFilename = strrchr(pPath, '/');
    if(!pFilename)
        pFilename = pPath;
    else
        pFilename++;

    unsigned char* dataBuf = (unsigned char*)memalign(0x40, BUFFER_SIZE);
    if(!dataBuf) {
        return -1;
    }

    FILE *pReadFile = fopen(pPath, "rb");
    if(!pReadFile)
    {
        //console_printf(1, "Can't open file %s\n", pPath);
        return -2;
    }

    FILE *pWriteFile = fopen(output_path, "wb");
    if(!pWriteFile)
    {
        console_printf(1, "Can't open write file %s\n", output_path);
        fclose(pReadFile);
        return -3;
    }

	console_printf(1, "0x%X - %s", 0, pPathShort);

    unsigned int size = 0;
    unsigned int ret;
    uint32_t passedMs = 1;
    OSTime startTime = OSGetTime();

    // Copy rpl in memory
    while ((ret = fread(dataBuf, 0x1, BUFFER_SIZE, pReadFile)) > 0)
    {
        passedMs = (uint32_t)OSTicksToMilliseconds(OSGetTime() - startTime);
        if(passedMs == 0)
            passedMs = 1; // avoid 0 div

        fwrite(dataBuf, 0x01, ret, pWriteFile);
        size += ret;
        console_printf(0, " %s - 0x%X (%i kB/s) (%s)\r", pFilename, size, (uint32_t)(((uint64_t)size * 1000) / ((uint64_t)1024 * passedMs)), pPathShort);
    }

    fclose(pWriteFile);
    fclose(pReadFile);
    free(dataBuf);
    return 0;
}

int DumpDir(char *pPath, const char * target_path)
{
    struct dirent *dirent = NULL;
    DIR *dir = NULL;

    dir = opendir(pPath);
    if (dir == NULL)
    {
        console_printf(1, "Can't open %s\n", pPath);
        return -1;
    }

    {
        char *targetPath = (char*)malloc(PATH_MAX);
        if(!targetPath)
        {
            console_printf(1, "no memory\n");
            closedir(dir);
            return -1;
        }

		char *pPathShort = strstr(pPath, "0005000");
		if (pPathShort != NULL)
		{
			pPathShort += 17;
		}
		else
		{
			pPathShort = strchr(pPath, '/');
		}

		snprintf(targetPath, PATH_MAX, "%s%s", target_path, pPathShort);
        CreateSubfolder(targetPath);
        free(targetPath);
    }

    while ((dirent = readdir(dir)) != 0)
    {
        if(strcmp(dirent->d_name, "..") == 0 || strcmp(dirent->d_name, ".") == 0)
            continue;

        if(CheckCancel())
            break;

        int len = strlen(pPath);
        snprintf(pPath + len, PATH_MAX - len, "/%s", dirent->d_name);

		char *pPathShort = strstr(pPath, "0005000");
		if (pPathShort != NULL)
		{
			pPathShort += 17;
		}
		else
		{
			pPathShort = strchr(pPath, '/');
			if (!pPathShort)
			{
				console_printf(1, "Error on %s\n", pPath);
				continue;
			}
			if (pPathShort[1] == '/')
				pPathShort++;
		}

        if(dirent->d_type & DT_DIR)
        {
            DumpDir(pPath, target_path);
        }
        else
        {
            char *targetPath = (char*)malloc(PATH_MAX);
            snprintf(targetPath, PATH_MAX, "%s%s", target_path, pPathShort);

            DumpFile(pPath, targetPath);
            free(targetPath);
        }
        pPath[len] = 0;
    }

    closedir(dir);
    return 0;
}

int StartDump(char *pPath, const char * target_path, const int selectedItem)
{
    char *targetPath = (char*)malloc(PATH_MAX);
    char *sdPath = (char*)malloc(PATH_MAX);

	if (targetPath && sdPath) 
	{
		strcpy(sdPath, target_path);
		if (selectedItem < 3)
			strcat(sdPath, "/game");
		else
			strcat(sdPath, "/update");
		
		uint32_t i;
		for (i = 0; i < 3; i++)
		{
			strcpy(targetPath, pPath);
			strcat(targetPath, game_ids[i]);
			
			DIR *dir = NULL;
			dir = opendir(targetPath);
			if (dir != NULL)
			{
				closedir(dir);
				DumpDir(targetPath, sdPath);
				break;
			}
		}

		free(sdPath);
		free(targetPath);
	}

	return 0;
}
