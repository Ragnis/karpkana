/*
 Copyright (C) 2009 Giacomo Spigler
 2013 - George Jordanov - improve in performance for HSV conversion and improvements

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))



#include "Camera.h"

#ifdef USE_OPENCV
#include <cv.h>
#include "opencv2/core/core.hpp"
#endif

static void errno_exit (const char *           s)
{
        fprintf (stderr, "%s error %d, %s\n",
                 s, errno, strerror (errno));

        exit (EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg)
{
        int r,itt=0;

        do {
		r = ioctl (fd, request, arg);
		itt++;
	}
        while ((-1 == r) && (EINTR == errno) && (itt<100));

        return r;
}
#ifdef USE_LOOKUP
void Camera::genYUVtoRGBLookups()
{
    int iyv, iyu, iy2v, iy2u;
    int i, j;
    for(i=0; i<256; i++){
        for(j=0; j<256; j++){
                iyv= i + (1.370705 * (j-128));  //Red
                iyu= i + (1.732446 * (j-128)); //Blue
                iy2v= (i/2) - (0.698001 * (j-128));//Green 1/2
                iy2u= (i/2) - (0.337633 * (j-128));//Green 1/2

                if(iyv > 255) iyv = 255;
                //if(iy2v + iy2u > 255) { iy2v = 127; iy2u = 128;}
                if(iyu > 255) iyu = 255;
                if(iyv < 0) iyv = 0;
                //if(iy2v + iy2u < 0) { iy2v = 0; iy2u = 0;}
                if(iyu < 0) iyu = 0;
                
                yv[i][j] = (unsigned char) iyv;
                yu[i][j] = (unsigned char) iyu;
                //Int
                y2v[i][j] = iy2v;
                y2u[i][j] = iy2u;
        }
    }
}
#endif
Camera::Camera(const char *n, int w, int h, int f) {
    this->initialised = false;
    this->StartCamera(n, w, h, f);
}


void Camera::StartCamera(const char *n, int w, int h, int f) {
    if(initialised == false){
        name=n;
        width=w;
        height=h;
        fps=f;

        w2=w/2;


        io=IO_METHOD_MMAP;

        data=(unsigned char *)malloc(w*h*4);
        
#ifdef USE_LOOKUP
        //Generate Lookup
        genYUVtoRGBLookups();
#endif
        
        this->Open();
        this->Init();  
        this->Start();

        initialised = true;
    }
}


Camera::~Camera() {
  this->StopCam();
}

void Camera::StopCam()
{
  if (initialised) {
    this->Stop();
    this->UnInit();
    this->Close();

    free(data);
    initialised = false;
  }
}

void Camera::Open() {
  struct stat st;
  if(-1==stat(name, &st)) {
    fprintf(stderr, "Cannot identify '%s' : %d, %s\n", name, errno, strerror(errno));
    exit(1);
  }

  if(!S_ISCHR(st.st_mode)) {
    fprintf(stderr, "%s is no device\n", name);
    exit(1);
  }

  fd=open(name, O_RDWR | O_NONBLOCK, 0);

  if(-1 == fd) {
    fprintf(stderr, "Cannot open '%s': %d, %s\n", name, errno, strerror(errno));
    exit(1);
  }

}

void Camera::Close() {
  if(-1==close(fd)) {
    errno_exit("close");
  }
  fd=-1;

}

void Camera::Init() {
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if(-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s is no V4L2 device\n",name);
      exit(1);
    } else {
       errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "%s is no video capture device\n", name);
    exit(1);
  }

  switch(io) {
    case IO_METHOD_READ:
      if(!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf(stderr, "%s does not support read i/o\n", name);
        exit (1);
      }

      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
      fprintf (stderr, "%s does not support streaming i/o\n", name);
      exit(1);
    }

    break;
  }


  CLEAR (cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if(0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if(-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
        }
      }
    } else {
      /* Errors ignored. */
    }

    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;


  if(-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
    errno_exit ("VIDIOC_S_FMT");



/*
struct v4l2_standard s;
s.name[0]='A';
s.frameperiod.numerator=1;
s.frameperiod.denominator=fps;

if(-1==xioctl(fd, VIDIOC_S_STD, &s))
  errno_exit("VIDIOC_S_STD");
*/


struct v4l2_streamparm p = { 0 };
p.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
//p.parm.capture.capability=V4L2_CAP_TIMEPERFRAME;
//p.parm.capture.capturemode=V4L2_MODE_HIGHQUALITY;
p.parm.capture.timeperframe.numerator=1;
p.parm.capture.timeperframe.denominator=fps;
p.parm.output.timeperframe.numerator=1;
p.parm.output.timeperframe.denominator=fps;
//p.parm.output.outputmode=V4L2_MODE_HIGHQUALITY;
//p.parm.capture.extendedmode=0;
//p.parm.capture.readbuffers=n_buffers;


if(-1==xioctl(fd, VIDIOC_S_PARM, &p))
  errno_exit("VIDIOC_S_PARM");

  //default values, mins and maxes
  struct v4l2_queryctrl queryctrl;

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_BRIGHTNESS;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("brightness error\n");
    } else {
      printf("brightness is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("brightness is not supported\n");
  }
  mb=queryctrl.minimum;
  Mb=queryctrl.maximum;
  db=queryctrl.default_value;


  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_CONTRAST;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("contrast error\n");
    } else {
      printf("contrast is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("contrast is not supported\n");
  }
  mc=queryctrl.minimum;
  Mc=queryctrl.maximum;
  dc=queryctrl.default_value;


  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_SATURATION;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("saturation error\n");
    } else {
      printf("saturation is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("saturation is not supported\n");
  }
  ms=queryctrl.minimum;
  Ms=queryctrl.maximum;
  ds=queryctrl.default_value;


  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_HUE;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("hue error\n");
    } else {
      printf("hue is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("hue is not supported\n");
  }
  mh=queryctrl.minimum;
  Mh=queryctrl.maximum;
  dh=queryctrl.default_value;


  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_HUE_AUTO;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("hueauto error\n");
    } else {
      printf("hueauto is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("hueauto is not supported\n");
  }
  ha=queryctrl.default_value;


  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_SHARPNESS;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      //perror ("VIDIOC_QUERYCTRL");
      //exit(EXIT_FAILURE);
      printf("sharpness error\n");
    } else {
      printf("sharpness is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("sharpness is not supported\n");
  }
  msh=queryctrl.minimum;
  Msh=queryctrl.maximum;
  dsh=queryctrl.default_value;

//TODO: TO ADD SETTINGS
//here should go custom calls to xioctl

//END TO ADD SETTINGS

  /* Note VIDIOC_S_FMT may change width and height. */

  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if(fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if(fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;

  switch(io) {
    case IO_METHOD_READ:
      init_read(fmt.fmt.pix.sizeimage);
      break;

    case IO_METHOD_MMAP:
      init_mmap();
      break;

    case IO_METHOD_USERPTR:
      init_userp(fmt.fmt.pix.sizeimage);
      break;
    }

}

void Camera::init_userp(unsigned int buffer_size) {
/*
  struct v4l2_requestbuffers req;
  unsigned int page_size;

  page_size = getpagesize();
  buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

  CLEAR (req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_USERPTR;

  if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
     fprintf (stderr, "%s does not support user pointer i/o\n", name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc (4, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = memalign (page_size,
                                                     buffer_size);

                if (!buffers[n_buffers].start) {
    			fprintf (stderr, "Out of memory\n");
            		exit (EXIT_FAILURE);
		}
        }


*/
}

void Camera::init_mmap() {
  struct v4l2_requestbuffers req;

  CLEAR (req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_MMAP;

  if(-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
    if(EINVAL == errno) {
      fprintf (stderr, "%s does not support memory mapping\n", name);
      exit (1);
    } else {
      errno_exit ("VIDIOC_REQBUFS");
    }
  }

  if(req.count < 2) {
    fprintf (stderr, "Insufficient buffer memory on %s\n", name);
    exit(1);
  }

  buffers = (buffer *)calloc(req.count, sizeof (*buffers));

  if(!buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }

  for(n_buffers = 0; n_buffers < (int)req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR (buf);

    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = n_buffers;

    if(-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
      errno_exit ("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap (NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

    if(MAP_FAILED == buffers[n_buffers].start)
      errno_exit ("mmap");
  }

}

void Camera::init_read (unsigned int buffer_size) {
/*
        buffers = calloc (1, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);

	if (!buffers[0].start) {
    		fprintf (stderr, "Out of memory\n");
            	exit (EXIT_FAILURE);
	}
*/
}

void Camera::UnInit() {
  unsigned int i;

  switch(io) {
    case IO_METHOD_READ:
      free (buffers[0].start);
      break;

    case IO_METHOD_MMAP:
      for(i = 0; i < (unsigned int)n_buffers; ++i)
        if(-1 == munmap (buffers[i].start, buffers[i].length))
          errno_exit ("munmap");
      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < (unsigned int)n_buffers; ++i)
        free (buffers[i].start);
      break;
  }

  free (buffers);
}

void Camera::Start() {
  unsigned int i;
  enum v4l2_buf_type type;

  switch(io) {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
      for(i = 0; i < (unsigned int)n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
          errno_exit ("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

      break;

    case IO_METHOD_USERPTR:
      for(i = 0; i < (unsigned int)n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = i;
        buf.m.userptr	= (unsigned long) buffers[i].start;
        buf.length      = buffers[i].length;

        if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
          errno_exit ("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

      break;
    }

}

void Camera::Stop() {
  enum v4l2_buf_type type;

  switch(io) {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
        errno_exit ("VIDIOC_STREAMOFF");

      break;
  }

}

unsigned char *Camera::Get() {
  struct v4l2_buffer buf;

  switch(io) {
    case IO_METHOD_READ:
/*
    		if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
            		switch (errno) {
            		case EAGAIN:
                    		return 0;

			case EIO:


			default:
				errno_exit ("read");
			}
		}

    		process_image (buffers[0].start);
*/
      break;

    case IO_METHOD_MMAP:
	{
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if(-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;
          case EIO:
          default:
            return 0; //errno_exit ("VIDIOC_DQBUF");
        }
      }

      assert(buf.index < (unsigned int)n_buffers);

      //memcpy(data, (unsigned char *)buffers[buf.index].start, buffers[buf.index].length);

	  ssize_t max = buffers[buf.index].length - 1;
	  unsigned char *end = (unsigned char *) buffers[buf.index].start + max;

	  for (ssize_t i = 0; i < (width * height) / 2; ++i)
	  {
		data[(i * 4)] = *(end - (i * 4) - 1);
		data[(i * 4) + 1] = *(end - (i * 4) - 2);
		data[(i * 4) + 2] = *(end - (i * 4) - 3);
		data[(i * 4) + 3] = *(end - (i * 4));
	  }

      if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
        return 0; //errno_exit ("VIDIOC_QBUF");

    return data;


	}
    break;

    case IO_METHOD_USERPTR:
/*
		CLEAR (buf);

    		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:


			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long) buffers[i].start
			    && buf.length == buffers[i].length)
				break;

		assert (i < n_buffers);

    		process_image ((void *) buf.m.userptr);

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
*/
      break;
  }

  return 0;
}

bool Camera::Update(unsigned int t, int timeout_ms) {
  bool grabbed = false;
  int grab_time_uS = 0;
  while (!grabbed) {
    if ((!grabbed) && (this->Get()!=0)) grabbed = true;
    if (!grabbed) {
      usleep(t);
      grab_time_uS+=(int)t;
      if (grab_time_uS > timeout_ms * 1000) {
        break;
      }
    }
  }

  return grabbed;

}

bool Camera::Update(Camera *c2, unsigned int t, int timeout_ms) {
  bool left_grabbed = false;
  bool right_grabbed = false;
  int grab_time_uS = 0;
  while (!(left_grabbed && right_grabbed)) {
    if ((!left_grabbed) && (this->Get()!=0)) left_grabbed = true;
    if ((!right_grabbed) && (c2->Get()!=0)) right_grabbed = true;
    if (!(left_grabbed && right_grabbed)) {
      usleep(t);
      grab_time_uS+=(int)t;
      if (grab_time_uS > timeout_ms * 1000) {
        break;
      }
    }
  }

  return left_grabbed & right_grabbed;

}

#ifdef USE_OPENCV
void Camera::toIplImage(IplImage *l) {
  unsigned char *l_=(unsigned char *)l->imageData;


  for(int x=0; x<w2; x++) {
    for(int y=0; y<height; y++) {
      int y0, y1, u, v; //y0 u y1 v

      int i=(y*w2+x)*4;
      y0=data[i];
      u=data[i+1];
      y1=data[i+2];
      v=data[i+3];

#ifndef USE_LOOKUP
      int r, g, b;

      r = y0 + (1.370705 * (v-128));
      g = y0 - (0.698001 * (v-128)) - (0.337633 * (u-128));
      b = y0 + (1.732446 * (u-128));

      if(r > 255) r = 255;
      if(g > 255) g = 255;
      if(b > 255) b = 255;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;

      i=(y*l->width+2*x)*3;
      l_[i] = (unsigned char)(b); //B
      l_[i+1] = (unsigned char)(g); //G
      l_[i+2] = (unsigned char)(r); //R

      r = y1 + (1.370705 * (v-128));
      g = y1 - (0.698001 * (v-128)) - (0.337633 * (u-128));
      b = y1 + (1.732446 * (u-128));

      if(r > 255) r = 255;
      if(g > 255) g = 255;
      if(b > 255) b = 255;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;

      l_[i+3] = (unsigned char)(b); //B
      l_[i+4] = (unsigned char)(g); //G
      l_[i+5] = (unsigned char)(r); //R
#else
      int g;
      i=(y*l->width+2*x)*3;
      
      g=y2u[y0][u] + y2v[y0][v];
      if(g>255){g=255;}
      if(g<0){g=0;}
      l_[i] = yu[y0][u];
      l_[i+1] = g;
      l_[i+2] = yv[y0][v];
      
      g=y2u[y1][u] + y2v[y1][v];
      if(g>255){g=255;}
      if(g<0){g=0;}
      l_[i+3] = yu[y1][u];
      l_[i+4] = g;
      l_[i+5] = yv[y1][v];
#endif
    }
  }

}
void Camera::toGrayScaleIplImage(IplImage *l){
    unsigned char *l_=(unsigned char *)l->imageData;
    
    for (int x = 0; x < w2; x++) {
        for (int y = 0; y < height; y++) {
            int y0, y1;
            
            int i = (y * w2 + x)*4;
            y0 = data[i];
            y1 = data[i + 2];

            i = (y * l->width + 2 * x)*1;
            l_[i] = (unsigned char) (y0);
            l_[i + 1] = (unsigned char) (y1);
        }
    }
}
void Camera::toMat(cv::Mat& m) {
  unsigned char *m_ = (unsigned char *)(m.data);


  for(int x=0; x<w2; x++) {
    for(int y=0; y<height; y++) {
      int y0, y1, u, v; //y0 u y1 v

      int i=(y*w2+x)*4;
      y0=data[i];
      u=data[i+1];
      y1=data[i+2];
      v=data[i+3];
            
#ifndef USE_LOOKUP
      int r, g, b;

      r = y0 + (1.370705 * (v-128));
      g = y0 - (0.698001 * (v-128)) - (0.337633 * (u-128));
      b = y0 + (1.732446 * (u-128));

      if(r > 255) r = 255;
      if(g > 255) g = 255;
      if(b > 255) b = 255;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;

      i = y*m.cols*m.channels() + x*2*m.channels();  
      m_[i] = (unsigned char)(b); //B
      m_[i+1] = (unsigned char)(g); //G
      m_[i+2] = (unsigned char)(r); //R

      r = y1 + (1.370705 * (v-128));
      g = y1 - (0.698001 * (v-128)) - (0.337633 * (u-128));
      b = y1 + (1.732446 * (u-128));

      if(r > 255) r = 255;
      if(g > 255) g = 255;
      if(b > 255) b = 255;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;

      m_[i+3] = (unsigned char)(b); //B
      m_[i+4] = (unsigned char)(g); //G
      m_[i+5] = (unsigned char)(r); //R
#else
      int g;
      
      i = y*m.cols*m.channels() + x*2*m.channels();  

      g=y2u[y0][u] + y2v[y0][v];
      if(g>255){g=255;}
      if(g<0){g=0;}
      m_[i] = yu[y0][u];
      m_[i+1] = (unsigned char)g;
      m_[i+2] = yv[y0][v];
      
      g=y2u[y1][u] + y2v[y1][v];
      if(g>255){g=255;}
      if(g<0){g=0;}
      m_[i+3] = yu[y1][u];
      m_[i+4] = g;
      m_[i+5] = yv[y1][v];
#endif
    }
  }
}   
void Camera::toGrayScaleMat(cv::Mat& m)
{   
    unsigned char *m_ = (unsigned char *)(m.data);
    
    for (int x = 0; x < w2; x++) {
        for (int y = 0; y < height; y++) {
            int y0, y1;

            int i = (y * w2 + x)*4;
            y0 = data[i];
            y1 = data[i + 2];
            
            i = y*m.cols*m.channels() + x*2*m.channels(); 
            m_[i + 0] = (unsigned char) (y0);
            m_[i + 1] = (unsigned char) (y1);
            
            //More clear in logic but slower solution
            //m.at<uchar>(y, (2*x)) = (unsigned char) (y0);
            //m.at<uchar>(y, (2*x)+1) = (unsigned char) (y1);
        }
    }
}
#endif

int Camera::minBrightness() {
  return mb;
}


int Camera::maxBrightness() {
  return Mb;
}


int Camera::defaultBrightness() {
  return db;
}


int Camera::minContrast() {
  return mc;
}


int Camera::maxContrast() {
  return Mc;
}


int Camera::defaultContrast() {
  return dc;
}


int Camera::minSaturation() {
  return ms;
}


int Camera::maxSaturation() {
  return Ms;
}


int Camera::defaultSaturation() {
  return ds;
}


int Camera::minHue() {
  return mh;
}


int Camera::maxHue() {
  return Mh;
}


int Camera::defaultHue() {
  return dh;
}


bool Camera::isHueAuto() {
  return ha;
}


int Camera::minSharpness() {
  return msh;
}


int Camera::maxSharpness() {
  return Msh;
}


int Camera::defaultSharpness() {
  return dsh;
}

int Camera::setBrightness(int v) {
  if(v<mb || v>Mb) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_BRIGHTNESS;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting brightness");
    return -1;
  }

  return 1;
}

int Camera::setContrast(int v) {
  if(v<mc || v>Mc) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_CONTRAST;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting contrast");
    return -1;
  }

  return 1;
}

int Camera::setSaturation(int v) {
  if(v<ms || v>Ms) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_SATURATION;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting saturation");
    return -1;
  }

  return 1;
}

int Camera::setHue(int v) {
  if(v<mh || v>Mh) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_HUE;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting hue");
    return -1;
  }

  return 1;
}

int Camera::setHueAuto(bool v) {
  if(v<mh || v>Mh) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_HUE_AUTO;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting hue auto");
    return -1;
  }

  return 1;
}

int Camera::setSharpness(int v) {
  if(v<mh || v>Mh) return -1;

  struct v4l2_control control;
  control.id = V4L2_CID_SHARPNESS;
  control.value = v;

  if(-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
    perror("error setting sharpness");
    return -1;
  }

  return 1;
}

