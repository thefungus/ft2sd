#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/dirent.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include <iosuhax_devoptab.h>
#include <iosuhax.h>
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "common/common.h"
#include "sd_dumper.h"

#define MAX_CONSOLE_LINES_TV    27
#define MAX_CONSOLE_LINES_DRC   18

static char * consoleArrayTv[MAX_CONSOLE_LINES_TV];
static char * consoleArrayDrc[MAX_CONSOLE_LINES_DRC];

static void console_print_pos(int x, int y, const char *format, ...)
{
	char * tmp = NULL;

	va_list va;
	va_start(va, format);
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
        if(strlen(tmp) > 79)
            tmp[79] = 0;

        OSScreenPutFontEx(0, x, y, tmp);
        OSScreenPutFontEx(1, x, y, tmp);

	}
	va_end(va);

	if(tmp)
        free(tmp);
}

void console_printf(int newline, const char *format, ...)
{
	char * tmp = NULL;

	va_list va;
	va_start(va, format);
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
	    if(newline)
        {
            if(consoleArrayTv[0])
                free(consoleArrayTv[0]);
            if(consoleArrayDrc[0])
                free(consoleArrayDrc[0]);

            for(int i = 1; i < MAX_CONSOLE_LINES_TV; i++)
                consoleArrayTv[i-1] = consoleArrayTv[i];

            for(int i = 1; i < MAX_CONSOLE_LINES_DRC; i++)
                consoleArrayDrc[i-1] = consoleArrayDrc[i];
        }
        else
        {
            if(consoleArrayTv[MAX_CONSOLE_LINES_TV-1])
                free(consoleArrayTv[MAX_CONSOLE_LINES_TV-1]);
            if(consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1])
                free(consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1]);

            consoleArrayTv[MAX_CONSOLE_LINES_TV-1] = NULL;
            consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1] = NULL;
        }

        if(strlen(tmp) > 79)
            tmp[79] = 0;

        consoleArrayTv[MAX_CONSOLE_LINES_TV-1] = (char*)malloc(strlen(tmp) + 1);
        if(consoleArrayTv[MAX_CONSOLE_LINES_TV-1])
            strcpy(consoleArrayTv[MAX_CONSOLE_LINES_TV-1], tmp);

        consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1] = (tmp);
	}
	va_end(va);

    // Clear screens
    OSScreenClearBufferEx(0, 0);
    OSScreenClearBufferEx(1, 0);


	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
    {
        if(consoleArrayTv[i])
            OSScreenPutFontEx(0, 0, i, consoleArrayTv[i]);
    }

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
    {
        if(consoleArrayDrc[i])
            OSScreenPutFontEx(1, 0, i, consoleArrayDrc[i]);
    }

	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);
}

int CheckCancel(void)
{
    int vpadError = -1;
    VPADData vpad;

    //! update only at 50 Hz, thats more than enough
    VPADRead(0, &vpad, 1, &vpadError);

    if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_B))
    {
        return 1;
    }
    return 0;
}

/* Entry point */
int Menu_Main(void)
{
    //!*******************************************************************
    //!                   Initialize function pointers                   *
    //!*******************************************************************
    //! do OS (for acquire) and sockets first so we got logging
    InitOSFunctionPointers();
    InitSocketFunctionPointers();

    log_init("192.168.178.3");
    log_print("Starting launcher\n");

    InitFSFunctionPointers();
    InitVPadFunctionPointers();

    log_print("Function exports loaded\n");

    //!*******************************************************************
    //!                    Initialize heap memory                        *
    //!*******************************************************************
    log_print("Initialize memory management\n");
    //! We don't need bucket and MEM1 memory so no need to initialize
    //memoryInitialize();

    //!*******************************************************************
    //!                        Initialize FS                             *
    //!*******************************************************************
    log_printf("Mount SD partition\n");
    mount_sd_fat("sd");

	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
        consoleArrayTv[i] = NULL;

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
        consoleArrayDrc[i] = NULL;

    VPADInit();

    // Prepare screen
    int screen_buf0_size = 0;

    // Init screen and screen buffers
    OSScreenInit();
    screen_buf0_size = OSScreenGetBufferSizeEx(0);
    OSScreenSetBufferEx(0, (void *)0xF4000000);
    OSScreenSetBufferEx(1, (void *)(0xF4000000 + screen_buf0_size));

    OSScreenEnableEx(0, 1);
    OSScreenEnableEx(1, 1);

    // Clear screens
    OSScreenClearBufferEx(0, 0);
    OSScreenClearBufferEx(1, 0);

    // Flip buffers
    OSScreenFlipBuffersEx(0);
    OSScreenFlipBuffersEx(1);

    int res = IOSUHAX_Open();
    if(res < 0)
    {
        console_printf(1, "IOSUHAX_open failed\n");
        sleep(2);
        return 0;
    }

    int fsaFd = IOSUHAX_FSA_Open();
    if(fsaFd < 0)
    {
        console_printf(1, "IOSUHAX_FSA_Open failed\n");
        sleep(2);
        return 0;
    }

    int vpadError = -1;
    VPADData vpad;

    int initScreen = 1;

	static const char* selection_paths[] =
	{
		"/dev/odd03",

		"/vol/storage_mlc01",
		"/vol/storage_usb01",

		"/vol/storage_mlc01",
		"/vol/storage_usb01"
	};

	static const char* selection_paths_description[] =
	{
		"GAME FILES: Disc",

		"GAME FILES: Digital game installed on System",
		"GAME FILES: Digital game installed on USB",

		"PATCH FILES: on System",
		"PATCH FILES: on USB"
	};

	static const char* selection_paths_targets[] =
	{
		"dev:/",

		"dev:/usr/title/00050000/",
		"dev:/usr/title/00050000/",

		"dev:/usr/title/0005000e/",
		"dev:/usr/title/0005000e/"
	};

    int selectedItem = 0;

    while(1)
    {
        //! update only at 50 Hz, thats more than enough
        VPADRead(0, &vpad, 1, &vpadError);

        if(initScreen)
        {
            initScreen = 0;

            //! free memory
            for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
            {
                if(consoleArrayTv[i])
                    free(consoleArrayTv[i]);
                consoleArrayTv[i] = 0;
            }

            for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
            {
                if(consoleArrayDrc[i])
                    free(consoleArrayDrc[i]);
                consoleArrayDrc[i] = 0;
            }

            // Clear screens
            OSScreenClearBufferEx(0, 0);
            OSScreenClearBufferEx(1, 0);


            console_print_pos(0, 0, "-- sm4sh2sd v0.2 by fungus --");

            console_print_pos(0, 3, "Dump one of GAME FILES first, and then dump one of PATCH FILES.");

            u32 i;
            for(i = 0; i < (sizeof(selection_paths) / 4); i++)
            {
                if(selectedItem == (int)i)
                {
                    console_print_pos(0, 5 + i, "--> %s", selection_paths_description[i]);
                }
                else
                {
                    console_print_pos(0, 5 + i, "    %s", selection_paths_description[i]);
                }
            }
            // Flip buffers
            OSScreenFlipBuffersEx(0);
            OSScreenFlipBuffersEx(1);
        }

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_DOWN))
        {
            selectedItem = (selectedItem + 1) % (sizeof(selection_paths) / 4);
            initScreen = 1;
            usleep(100000);
        }

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_UP))
        {
            selectedItem--;
            if(selectedItem < 0)
                selectedItem =  (sizeof(selection_paths) / 4) - 1;
            initScreen = 1;
            usleep(100000);
        }

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_A))
        {
            const char *dev_path = (selectedItem < 1) ? selection_paths[selectedItem] : NULL;
            const char *mount_path = (selectedItem >= 1) ? selection_paths[selectedItem] : "/vol/storage_ft_content";
			const char *target_path = selection_paths_targets[selectedItem];

            int res = mount_fs("dev", fsaFd, dev_path, mount_path);
            if(res < 0)
            {
                console_printf(1, "Mount of %s to %s failed", dev_path, mount_path);
            }
            else
            {
                char *targetPath = (char*)malloc(FS_MAX_FULLPATH_SIZE);
                if(targetPath)
                {
					strcpy(targetPath, target_path);
					if (selectedItem < 1) 
					{
						char *contentPath = (char*)malloc(FS_MAX_FULLPATH_SIZE);
						if (contentPath)
						{
							snprintf(contentPath, FS_MAX_FULLPATH_SIZE, "%s%s", targetPath, "content");

							DIR *dir = NULL;
							dir = opendir(contentPath);
							if (dir != NULL)
							{
								closedir(dir);
								free(contentPath);
								DumpDir(targetPath, "sd:/sm4sh2sd/game");
							}
							else
							{
								free(contentPath);
								unmount_fs("dev");
								int res = mount_fs("dev", fsaFd, "/dev/odd04", mount_path);
								if (res < 0)
								{
									console_printf(1, "Mount of %s to %s failed", "/dev/odd04", mount_path);
								}
								else
								{
									DumpDir(targetPath, "sd:/sm4sh2sd/game");
								}
							}
						}
					}
					else 
					{
						StartDump(targetPath, "sd:/sm4sh2sd", selectedItem);
					}
					free(targetPath);
                }
                unmount_fs("dev");
                console_printf(1, "Dump complete");
            }
            sleep(3);
            initScreen = 1;
        }

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_HOME))
        {
            break;
        }

		usleep(50000);
    }

    //! free memory
	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
    {
        if(consoleArrayTv[i])
            free(consoleArrayTv[i]);
    }

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
    {
        if(consoleArrayDrc[i])
            free(consoleArrayDrc[i]);
    }

    //!*******************************************************************
    //!                    Enter main application                        *
    //!*******************************************************************

    log_printf("Unmount SD\n");
    unmount_sd_fat("sd");

    IOSUHAX_FSA_Close(fsaFd);
    IOSUHAX_Close();

    log_printf("Release memory\n");
    //memoryRelease();
    log_deinit();

    return EXIT_SUCCESS;
}

