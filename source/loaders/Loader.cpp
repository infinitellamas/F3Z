#include "loaders/Loader.h"


#define TINYOBJLOADER_IMPLEMENTATION 
#include "library/tinyobj_loader.h"

Loader::Loader(){

}

Loader::~Loader() {

}

int Loader::texConvertFlags =
(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

Bitmap Loader::loadToBitmap(std::string filepath) {
	Bitmap picture;
	std::string fullPath = "romfs:" + filepath;
	std::vector<unsigned char> image; //the raw pixels
	 
	// decode
	if (lodepng::decode(image, picture.width, picture.height, fullPath))
		Error::throwError("Cannot load or decode PNG file " + fullPath);
	picture.datalen = picture.width * picture.height;
	picture.pixels = (unsigned char*)linearAlloc(sizeof(unsigned char) * picture.datalen*4);
	
	//copy into struct
	memcpy(picture.pixels, image.data(), sizeof(unsigned char) * picture.datalen * 4);
	
	return picture;
}


void Loader::bitmapToTiled(Bitmap inImage, C3D_Tex* outImage) {
	bitmapToTiled(inImage.pixels, inImage.width, inImage.height, outImage);
}
void Loader::bitmapToTiled(unsigned char* inImage, unsigned long inImgWidth, unsigned long inImgHeight, C3D_Tex* outImage) {

	// Create storage for swapping and tiling
	u8 *gpusrc = (u8*)linearAlloc(inImgWidth*inImgHeight * 4);

	// GX_DisplayTransfer needs input buffer in linear RAM
	u8* src = inImage; u8 *dst = gpusrc;

	// lodepng outputs big endian rgba so we need to convert
	for (unsigned int i = 0; i<inImgWidth*inImgHeight; i++) {
		int r = *src++;
		int g = *src++;
		int b = *src++;
		int a = *src++;

		*dst++ = a;
		*dst++ = b;
		*dst++ = g;
		*dst++ = r;
	}

	// ensure data is in physical ram
	GSPGPU_FlushDataCache(gpusrc, inImgWidth*inImgHeight * 4);

	// Load the texture and bind it to the first texture unit
	C3D_TexInit(outImage, inImgWidth, inImgHeight, GPU_RGBA8);

	// Convert image to 3DS tiled texture format
	C3D_SafeDisplayTransfer((u32*)gpusrc, GX_BUFFER_DIM(inImgWidth, inImgHeight), (u32*)outImage->data, GX_BUFFER_DIM(inImgWidth, inImgHeight), texConvertFlags);
	gspWaitForPPF();

	linearFree(gpusrc);
}

Model Loader::loadOBJ(std::string filepath) {
	Model model;

	std::string inputfile = "romfs:" + filepath;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, inputfile.c_str());
	if (!ret) {
		Error::throwError("Cannot open or parse OBJ file: " + inputfile);
	}
	
	std::vector <Model::vertex> vertices;
	Model::vertex vert;
	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				vert.position[0] = attrib.vertices[3 * idx.vertex_index + 0];
				vert.position[1] = attrib.vertices[3 * idx.vertex_index + 1];
				vert.position[2] = attrib.vertices[3 * idx.vertex_index + 2];
				vert.normal[0] = attrib.normals[3 * idx.normal_index + 0];
				vert.normal[1] = attrib.normals[3 * idx.normal_index + 1];
				vert.normal[2] = attrib.normals[3 * idx.normal_index + 2];
				vert.texcoord[0] = attrib.texcoords[2 * idx.texcoord_index + 0];
				vert.texcoord[1] = attrib.texcoords[2 * idx.texcoord_index + 1];
				vertices.push_back(vert);
			}
			index_offset += fv;
		}
	}

	Model::vertex* vBuffer = (Model::vertex*)linearAlloc(sizeof(Model::vertex) * vertices.size());
	memcpy(vBuffer, vertices.data(), sizeof(Model::vertex) * vertices.size());
	model.setVertices(vBuffer, vertices.size());
	linearFree(vBuffer);

	return model;
}

std::vector<StaticTile> Loader::loadStaticTiles(std::string islandName) {
	std::vector<StaticTile> islandTiles;
	std::string tilesetJson;
	std::string islandJson;


	/* Load tileset config */
	FILE* tilesetJsonFile = Globals::fs_.openFile(Drive::FS_ROMFS, "config/tiles.json", "r");
	if (!tilesetJsonFile)
		Error::throwError("Cannot open tileset config file\n<romfs:config/tiles.json>");

	/* Load island config */
	FILE* islandJsonFile = Globals::fs_.openFile(Drive::FS_ROMFS, "islands/" + islandName + ".json", "r");
	if (!islandJsonFile)
		Error::throwError("Cannot open island config file\n<romfs:islands/" + islandName + " .json>");

	/* Read json */
	fseek(tilesetJsonFile, 0, SEEK_END);
	long tilesetFileSize = ftell(tilesetJsonFile);
	fseek(tilesetJsonFile, 0, SEEK_SET);
	tilesetJson.resize(tilesetFileSize);
	fread(&tilesetJson[0], sizeof(char), tilesetFileSize, tilesetJsonFile);

	fseek(islandJsonFile, 0, SEEK_END);
	long islandFileSize = ftell(islandJsonFile);
	fseek(islandJsonFile, 0, SEEK_SET);
	islandJson.resize(islandFileSize);
	fread(&islandJson[0], sizeof(char), islandFileSize, islandJsonFile);

	Json::Reader reader;

	Json::Value
		tilesetRoot,
		islandRoot;

	bool valid = reader.parse(tilesetJson, tilesetRoot);
	if(!valid)
		Error::throwError("<romfs:config/tiles.json> had invalid json:\n" + reader.getFormattedErrorMessages());

	valid = reader.parse(islandJson, islandRoot);
	if (!valid)
		Error::throwError("<romfs:islands/" + islandName + ".json> had invalid json:\n" + reader.getFormattedErrorMessages());

	for (auto itr : islandRoot["tileset"]) {
		
		/* Only do static tiles */
		if (tilesetRoot["tileData"]["static"].isMember(itr.asString())) {
			StaticTile t;
			t.canRotate = tilesetRoot["tileData"]["static"][itr.asString()].isMember("canRotate") ? tilesetRoot["tileData"]["static"][itr.asString()]["canRotate"].asBool() : false;

			std::string tileTexFilePath = tilesetRoot["tileData"]["static"][itr.asString()].isMember("texPath") ? tilesetRoot["tileData"]["static"][itr.asString()]["texPath"].asString() : "";
			if (tileTexFilePath.size() < 1)
				Error::throwError(itr.asString() + " has no texPath.");
			
			Bitmap b = loadToBitmap(tileTexFilePath);
			C3D_Tex* tex = new C3D_Tex;
			bitmapToTiled(b, tex);
			t.tileTex = tex;
			
			islandTiles.push_back(t);
		}
	}

	fclose(islandJsonFile);
	fclose(tilesetJsonFile);
	return islandTiles;

}
