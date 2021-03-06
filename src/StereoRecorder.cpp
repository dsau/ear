/************************************************************************
 *                                                                      *
 * This file is part of EAR: Evaluation of Acoustics using Ray-tracing. *
 *                                                                      *
 * EAR is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * EAR is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with EAR.  If not, see <http://www.gnu.org/licenses/>.         *
 *                                                                      *
 ************************************************************************/

#include <sstream>
#include <iostream>
#include <fstream>

#include "../lib/wave/WaveFile.h"
#include "SoundFile.h"
#include "StereoRecorder.h"

void StereoRecorder::setLocation(gmtl::Point3f& loc) { location = loc; }
std::string StereoRecorder::getFilename() { return filename; }
void StereoRecorder::setFilename(const std::string& s) { filename = s; }
int StereoRecorder::trackCount() { return 2; }

StereoRecorder::StereoRecorder(bool fromFile) {
	is_truncated = is_processed = false;
	if ( fromFile ) {
		stamped_offset = 0;
		Read(false);
		assertid("OUT2");
		filename = ReadString();
		float t = ReadFloat();
		if ( Datatype::PeakId() == "anim" ) {
			animation = new Animated<gmtl::Point3f>();
		} else {
			setLocation(ReadPoint());
			animation = 0;
		}
		if ( Datatype::PeakId() == "anim" ) {
			right_ear_animation = new Animated<gmtl::Vec3f>();
		} else {
			right_ear = ReadVec();
			right_ear_animation = 0;
		}
		head_size = ReadFloat();
		head_absorption = ReadVec();
		for ( unsigned int i = 0; i < 3; ++ i ) {
			head_absorption[i] = std::max(0.0f,powf(1.0f-head_absorption[i],4));
		}
		std::cout << this->toString();
	} else {
		animation = 0;
		right_ear_animation = 0;
		filename = "";
	}
	has_samples = save_processed = false;
	tracks.push_back(new RecorderTrack());
	tracks.push_back(new RecorderTrack());
}
Recorder* StereoRecorder::getBlankCopy(int secs) {
	StereoRecorder* r = new StereoRecorder(false);
	r->stamped_offset = 0;
	r->filename = filename;
	r->location = location;
	r->animation = animation;
	r->save_processed = save_processed;
	r->right_ear = right_ear;
	r->right_ear_animation = right_ear_animation;
	r->head_size = head_size;
	r->head_absorption = head_absorption;
	return r;
}
bool StereoRecorder::Save(const std::string& fn, bool norm, float norm_max) {
	WaveFile w;
	const RecorderTrack& left  = *(save_processed ? processed_tracks[0] : tracks[0]);
	const RecorderTrack& right = *(save_processed ? processed_tracks[1] : tracks[1]);
	w.FromFloat(&left[0],&right[0],left.getLength(),right.getLength(),norm);
	w.Save(fn.c_str());
	return true;
}
bool StereoRecorder::Save() {
	return Save(filename,false);
}
inline void StereoRecorder::_Sample(int i, float v, int channel) {
	if ( i < 0 ) return;
	this->tracks[channel]->operator [](i) += v;
	has_samples = true;
}
void StereoRecorder::Record(const gmtl::Vec3f& dir, float a, float t, float dist, int band, int kf) {
	const float dot = gmtl::dot(dir,getRightEar(kf));
	const float time_difference = head_size / 343.0f;

	const int s_right = (int) ((t-(dot*time_difference))*44100.0);
	const int s_left = (int) ((t+(dot*time_difference))*44100.0);

	const float width = sqrt(dist);
	const float ampl = 2.0f * a / width;
	
	float ampl_left = ampl;
	float ampl_right = ampl;

	const float intensity_difference = fabs(dot);

	// Interaural intensity differences are higher for the high frequency bands
	const float factor = powf(head_absorption[band],intensity_difference*head_size);
	if ( dot < 0 ) {
		ampl_right *= factor*factor;
	} else {
		ampl_left *= factor*factor;
	}
	
	const int w = (int) ceil(width);
	const float step_left = ampl_left / w;
	const float step_right = ampl_right / w;

	for ( int i = 0; i < w; ++ i ) {
		_Sample(i+s_left,ampl_left,0);
		ampl_left -= step_left;
		_Sample(i+s_right,ampl_right,1);
		ampl_right -= step_right;
	}
}
void StereoRecorder::setLocation(gmtl::Point3f p) {
	location = p;
}
bool StereoRecorder::isAnimated() { return animation > 0; }
const gmtl::Point3f& StereoRecorder::getLocation(int i) const {
	if ( i >= 0 && animation > 0 ) {
		return animation->Evaluate(i);
	} else {
		return location;
	}
}
const gmtl::Vec3f& StereoRecorder::getRightEar(int i) {
	if ( i >= 0 && right_ear_animation > 0 ) {
		return right_ear_animation->Evaluate(i);
	} else {
		return right_ear;
	}
}
float StereoRecorder::getSegmentLength(int i) {
	return animation->SegmentLength(i);
}
std::string StereoRecorder::toString() {
	std::stringstream ss;
	ss << std::setprecision(std::cout.precision()) << std::fixed;
	ss << "Recorder" << std::endl << " +- stereo" << std::endl << " +- location: ";
	if ( this->animation ) {
		ss << this->animation->toString();
	} else {
		ss << this->location;
	}
	ss << std::endl << " +- right: ";
	if ( this->right_ear_animation ) {
		ss << this->right_ear_animation->toString();
	} else {
		ss << this->right_ear;
	}
	ss << std::endl << " +- head size: " << head_size << std::endl << " +- head absorption: " << head_absorption << std::endl;
	return ss.str();
}
Animated<gmtl::Point3f>* StereoRecorder::getAnimationData() {
	return animation;
}