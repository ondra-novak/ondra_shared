/*
 * worker.cpp
 *
 *  Created on: 30. 4. 2018
 *      Author: ondra
 */



#include <fstream>
#include "../worker.h"
#include "../future.h"


using namespace ondra_shared;

int main(int argc, char **argv) {

	std::vector<std::vector<unsigned char> > buffer;
	unsigned int threads = std::thread::hardware_concurrency();
	if (threads == 0) threads = 1;
	auto worker = Worker::create(threads);
	MTCounter rows;



	static const double left = -1.153;
	static const double right = -1.154;
	static const double top = 0.201;
	static const double bottom = 0.202;
	static const unsigned int sizeX = 10000;
	static const unsigned int sizeY = 10000;

	buffer.resize(sizeY);
	for (unsigned int i = 0; i < sizeY; i++) {
		std::vector<unsigned char> &row = buffer[i];
		row.resize(sizeX);
		rows.inc();
		worker >> [i,&buffer,&rows] {
			std::vector<unsigned char> &row = buffer[i];

			double y = top + ((bottom - top)*(i / double(sizeY)));
			for (unsigned int j = 0; j < sizeX; j++) {
				double x = left + ((right - left)*(j / double(sizeX)));
				double newRe = 0, newIm = 0, oldRe = 0, oldIm = 0;
				int i;
				for (i = 0; i < 255; i++)
				{
					oldRe = newRe;
					oldIm = newIm;
					double oldRe2 = oldRe * oldRe;
					double oldIm2 = oldIm * oldIm;
					if (oldRe2 + oldIm2 > 4) {
						i--; break;
					}
					newRe = oldRe2 - oldIm2+ x;
					newIm = 2 * oldRe * oldIm + y;
				}
				row[j] = (unsigned char)i;
			}
			rows.dec();
		};
	}
	rows.wait();
	worker = Worker();
	std::ofstream f("worker.pgm");
	f << "P2" << std::endl;
	f << sizeX << " " << sizeY << std::endl;
	f << "256" << std::endl;
	for (unsigned int i = 0; i < sizeX; i++) {
		for (unsigned int j = 0; j < sizeY; j++) {
			f << (int)buffer[i][j] << " ";
		}
		f << std::endl;
	}
}

