#include <AGNet.h>
#include <AGUtil.h>
#include <AGClientProcessor.h>
#include <AGProtocol.h>
#include <AGBufferReader.h>
#include <AGPalmProtocol.h>
#include <AGUserConfig.h>
#include <AGServerConfig.h>
#include <AGSyncCommon.h>
#include <AGCommandProcessor.h>
#include <AGDesktopInfoPalm.h>
#include <AGTimeoutMessageBox.h>
#include <AGMobileLinkSettings.h>
#include <MAL31UserConfig.h>


#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard.h>

typedef struct {

    AGDeviceInfo * deviceInfo;
    AGUserConfig * userConfig;
    AGServerConfig * currentServerConfig;
    AGClientProcessor * clientProcessor;
    AGPlatformCalls * platform;
    AGRecord * record;
    AGDBConfig * currentDb;
    AGCommandProcessor *commandProcessor;
    AGBool quit;
    
    /* Pilot-specific */
    AGBool currentDBIsResourceType;
    int pilot_rHandle;
    int pilot_RecIndex;
    recordid_t id;
#ifdef PILOT_LINK_0_12
    pi_buffer_t *pi_buf;    
#else
    uint8  *pilot_buffer;
    int pilot_buffer_size;
#endif
    
    /* Secure Network Library Stuff */
    AGBool          hasseclib;

    /* We're smuggling the GnomePilotConduit along for the ride */
    GnomePilotConduitStandard *conduit;

} PalmSyncInfo;

