/* $Id: mal-conduit.h 200 2001-08-02 08:47:44Z eskil $ */

#ifdef MC_DEBUG
#define LOG(format,args...) g_log (G_LOG_DOMAIN, \
                                   G_LOG_LEVEL_MESSAGE, \
                                   "mal-conduit: "##format, ##args)
#else
#define LOG(format,args...)
#endif


