/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2copy.cpp
** 
** Copy from a V4L2 capture device to an other V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

int stop=0;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	const char *out_devname = "/dev/video1";	
	const char *format = "JPEG";
	int c = 0;
	int width = 0, height = 0, fps = 0;
	V4l2IoType ioTypeIn  = IOTYPE_MMAP;
	V4l2IoType ioTypeOut = IOTYPE_MMAP;
	
	while ((c = getopt (argc, argv, "hv::W:H:F:f:rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++; break;
			case 'W':       if (optarg) width = atoi(optarg); break;
			case 'H':       if (optarg) height = atoi(optarg); break;
			case 'F':       if (optarg) fps = atoi(optarg); break;
			case 'f':       if (optarg) format = optarg; break;
			case 'r':	ioTypeIn  = IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = IOTYPE_READWRITE; break;			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] [-F fps] [-f format] [-r] [-w] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose" << std::endl;
				std::cout << "\t -vv           : very verbose" << std::endl;
				std::cout << "\t -W            : V4L2 capture width (default " << width << ")" << std::endl;
				std::cout << "\t -H            : V4L2 capture height (default " << height << ")" << std::endl;
				std::cout << "\t -F            : V4L2 capture frame per sec (default " << fps << ")" << std::endl;
				std::cout << "\t -f            : V4L2 pixel format in 4CC codes (default " << format << ")" << std::endl;
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device : V4L2 capture device (default " << in_devname << ")" << std::endl;
				std::cout << "\t dest_device   : V4L2 capture device (default " << out_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		in_devname = argv[optind];
		optind++;
	}	
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	

	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	if (strlen(format) != 4)
	{
		LOG(WARN) << "Invalid pixel format:" << format;
		return 0;
	}
	
	unsigned int fourcc = v4l2_fourcc(format[0], format[1], format[2], format[3]);
	V4L2DeviceParameters param(in_devname, fourcc, width, height, fps, ioTypeIn, verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, videoCapture->getFormat(), videoCapture->getWidth(), videoCapture->getHeight(), 0, ioTypeOut, verbose);
		V4l2Output* videoOutput = V4l2Output::create(outparam);
		char buffer[1 * 1024 * 1024]; // avoid creating buffer every time
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			timeval tv;
			
			LOG(NOTICE) << "Start Copying from " << in_devname << " to " << out_devname; 
			signal(SIGINT,sighandler);				
			while (!stop) 
			{
				tv.tv_sec=1;
				tv.tv_usec=0;
				int ret = videoCapture->isReadable(&tv);
				if (ret == 1)
				{
					// char buffer[videoCapture->getBufferSize()];
					int rsize = videoCapture->read(buffer, videoCapture->getBufferSize());
					if (rsize == -1)
					{
						LOG(NOTICE) << "stop " << strerror(errno); 
						stop=1;					
					}
					else
					{
						int wsize = videoOutput->write(buffer, rsize);
						// LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
					}
				}
				else if (ret == -1)
				{
					LOG(NOTICE) << "stop " << strerror(errno); 
					stop=1;
				}
			}
			delete videoOutput;
		}
		delete videoCapture;
	}
	
	return 0;
}
