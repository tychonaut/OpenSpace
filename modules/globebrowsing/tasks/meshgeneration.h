/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2017                                                               *
*                                                                                       *
* Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
* software and associated documentation files (the "Software"), to deal in the Software *
* without restriction, including without limitation the rights to use, copy, modify,    *
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
* permit persons to whom the Software is furnished to do so, subject to the following   *
* conditions:                                                                           *
*                                                                                       *
* The above copyright notice and this permission notice shall be included in all copies *
* or substantial portions of the Software.                                              *
*                                                                                       *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
****************************************************************************************/

#ifndef __OPENSPACE___MESH_GENERATION___H__
#define __OPENSPACE___MESH_GENERATION___H__

#include <vector>

#include <pcl/TextureMesh.h>

namespace {
	const std::string _outputPath = "";
}

namespace openspace {
namespace globebrowsing {
	class MeshGeneration {		
	public:
		struct PointCloudInfo {
			int _lines;
			int _cols;
			int _bands;
			int _bytes;
			std::vector<double> _roverOrigin;
		};

		static void generateMeshFromBinary(const std::string binary_path, std::string output_path);

	private: 
		//void writeObjFile();
		static void writeObjFile(const std::string filename, std::string output_path, const pcl::TextureMesh texMesh);
		static void writeMtlFile(const std::string filename, const std::string output_path, const pcl::TextureMesh texMesh);
		static MeshGeneration::PointCloudInfo readBinaryHeader(const std::string filename);
		static void readBinaryData(const std::string filename, std::vector<std::vector<float>> &xyz, const MeshGeneration::PointCloudInfo pci);
		static void writeTxtFile(const std::string filename, std::string output_path);
		static std::string correctPath(const std::string filename, std::string output_path);
	};
}
}

#endif //__OPENSPACE___MESH_GENERATION___H__