#include <vpad/input.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <iosuhax_devoptab.h>
#include <iosuhax.h>
#include "sd_dumper.h"
#include "console.h"

int CheckCancel(void)
{
    VPADReadError vpadError = -1;
    VPADStatus vpad;
    
    //! update only at 50 Hz, thats more than enough
    VPADRead(0, &vpad, 1, &vpadError);
    
    if(vpadError == 0 && ((vpad.hold | vpad.trigger) & VPAD_BUTTON_B))
    {
        return 1;
    }
    return 0;
}


/* Entry point */
int main(int argc, char **argv)
{
    ProcUIInit(&OSSavesDone_ReadyToRelease);
    ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &console_acquire, NULL, 100);
    ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, &console_release, NULL, 100);
    
    console_acquire(NULL);
    
    int res = IOSUHAX_Open(NULL);
    if(res < 0)
    {
        console_printf(1, "IOSUHAX_open failed\n");
        OSSleepTicks(OSSecondsToTicks(2));
        SYSLaunchMenu();
    }
    
    int fsaFd = IOSUHAX_FSA_Open();
    if(fsaFd < 0)
    {
        console_printf(1, "IOSUHAX_FSA_Open failed\n");
        OSSleepTicks(OSSecondsToTicks(2));
        SYSLaunchMenu();
    }

    VPADReadError vpadError = -1;
    VPADStatus vpad;

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
    ProcUIStatus status;

    while((status = ProcUIProcessMessages(TRUE)) != PROCUI_STATUS_EXITING)
    {
        if(status == PROCUI_STATUS_RELEASE_FOREGROUND)
            ProcUIDrawDoneRelease();
        if(status != PROCUI_STATUS_IN_FOREGROUND)
        {
            //! force screen redraw when we return in foreground
            initScreen = 1;
            continue;
        }

        //! update only at 50 Hz, thats more than enough
        VPADRead(0, &vpad, 1, &vpadError);

        if(initScreen)
        {
            initScreen = 0;

            // free memory
            console_cleanup();

            // Clear screens
            OSScreenClearBufferEx(0, 0);
            OSScreenClearBufferEx(1, 0);


            console_print_pos(0, 0, "-- sm4sh2sd v0.2 by fungus --");

            console_print_pos(0, 3, "Dump one of GAME FILES first, and then dump one of PATCH FILES.");

            uint32_t i;
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

        if(vpadError == 0 && ((vpad.hold | vpad.trigger) & VPAD_BUTTON_DOWN))
        {
            selectedItem = (selectedItem + 1) % (sizeof(selection_paths) / 4);
            initScreen = 1;            
            OSSleepTicks(OSMillisecondsToTicks(100));
        }
        
        if(vpadError == 0 && ((vpad.hold | vpad.trigger) & VPAD_BUTTON_UP))
        {
            selectedItem--;
            if(selectedItem < 0)
                selectedItem =  (sizeof(selection_paths) / 4) - 1;
            initScreen = 1;
            OSSleepTicks(OSMillisecondsToTicks(100));
        }

        if(vpadError == 0 && ((vpad.hold | vpad.trigger) & VPAD_BUTTON_A))
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
                char *targetPath = (char*)malloc(PATH_MAX);
                if(targetPath)
                {
                    strcpy(targetPath, target_path);
                    if (selectedItem < 1) 
                    {
                        char *contentPath = (char*)malloc(PATH_MAX);
                        if (contentPath)
                        {
                            snprintf(contentPath, PATH_MAX, "%s%s", targetPath, "content");

                            DIR *dir = NULL;
                            dir = opendir(contentPath);
                            if (dir != NULL)
                            {
                                closedir(dir);
                                free(contentPath);
                                DumpDir(targetPath, "fs:/vol/external01/sm4sh2sd/game");
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
                                    DumpDir(targetPath, "fs:/vol/external01/sm4sh2sd/game");
                                }
                            }
                        }
                    }
                    else 
                    {
                        StartDump(targetPath, "fs:/vol/external01/sm4sh2sd", selectedItem);
                    }
                    free(targetPath);
                }
                unmount_fs("dev");
                console_printf(1, "Dump complete");
            }
            OSSleepTicks(OSSecondsToTicks(3));
            initScreen = 1;
        }

        OSSleepTicks(OSMillisecondsToTicks(50));
    }

    IOSUHAX_FSA_Close(fsaFd);
    IOSUHAX_Close();

    console_cleanup();
    console_release(NULL);

    ProcUIShutdown();
    return 0;
}

