#include "VideoProcessor.h"

VideoProcessor::VideoProcessor() :
	debugImgMode(DEBUG_IMG_NONE),
	sequence(0),
	dataFresh(false)
{
	this->vision.initialize(CAPT_WIDTH, CAPT_HEIGHT);
	this->vision.loadOptions("config/colors.txt");
}

VideoProcessor::~VideoProcessor()
{
	this->vision.close();
}

void VideoProcessor::putRawFrame(unsigned char *data)
{
	std::lock_guard<std::mutex> lock(this->dataMutex);
	memcpy(&this->data, data, CAPT_WIDTH * CAPT_HEIGHT * 4);
	this->dataFresh = true;
}

VideoFrame *VideoProcessor::getFrame()
{
	image_pixel *cmImg;
	long begin = microtime();

	// Convert data to a format acceptable to CMVision
	{
		std::lock_guard<std::mutex> lock(this->dataMutex);

		if (!this->dataFresh)
		{
			return NULL;
		}

		cmImg = new image_pixel[(CAPT_WIDTH * CAPT_HEIGHT) / 2];

		for (size_t i = 0; i < (CAPT_WIDTH * CAPT_HEIGHT) / 2; i++)
		{
			cmImg[i].y1 = this->data[(i * 4)];
			cmImg[i].u = this->data[(i * 4) + 1];
			cmImg[i].y2 = this->data[(i * 4) + 2];
			cmImg[i].v = this->data[(i * 4) + 3];
		}

		this->dataFresh = false;
	}

	if (!this->vision.processFrame(cmImg))
	{
		perror("Processing frame");
		return NULL;
	}

	VideoFrame *vf = new VideoFrame(this->sequence++);

	for (size_t color = 0; color < REGION_NUM; color++)
	{
		CMVision::region *region = this->vision.getRegions(color);

		while (region)
		{
			VideoFrame::Blob *blob = VideoFrame::Blob::fromRegion(region);

			if (blob)
			{
				vf->blobs.push_back(blob);
			}

			region = region->next;
		}
	}

	if (this->debugImgMode == DEBUG_IMG_CLASSIFY)
	{
		rgb *debugImg = new rgb[CAPT_WIDTH * CAPT_HEIGHT];
		this->vision.testClassify(debugImg, cmImg);

		DebugLink::instance().image(vf->sequence, debugImg);
	}
	else if (this->debugImgMode == DEBUG_IMG_RAW)
	{
		rgb *debugImg = new rgb[CAPT_WIDTH * CAPT_HEIGHT];
		image_pixel yuyv;
		int r, g, b;

		for (size_t i = 0; i < CAPT_WIDTH * CAPT_HEIGHT; i++)
		{
			yuyv = cmImg[i / 2];

			r = yuyv.y1 + (1.370705 * (yuyv.v-128));
			g = yuyv.y1 - (0.698001 * (yuyv.v-128)) - (0.337633 * (yuyv.u-128));
			b = yuyv.y1 + (1.732446 * (yuyv.u-128));

			FORCE_RANGE(r, 0, 255);
			FORCE_RANGE(g, 0, 255);
			FORCE_RANGE(b, 0, 255);

			debugImg[i].red = (unsigned char) r;
			debugImg[i].green = (unsigned char) g;
			debugImg[i].blue = (unsigned char) b;
		}

		DebugLink::instance().image(vf->sequence, debugImg);
	}

	DebugLink::instance().fps(DebugLink::FPS_PROC, 1000000.0 / (microtime() - begin));

	delete[] cmImg;

	return vf;
}
