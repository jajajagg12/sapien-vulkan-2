#include <filesystem>
#include <set>
#include "svulkan2\shader\shaderManager.h"

namespace fs = std::filesystem;

namespace svulkan2 {
namespace shader {

inline std::string getOutTextureName(std::string variableName) {// remove "out" prefix
	return variableName.substr(3, std::string::npos);
}

inline std::string getInTextureName(std::string variableName) {// remove "sampler" prefix
	return variableName.substr(7, std::string::npos);
}

ShaderManager::ShaderManager(std::shared_ptr<RendererConfig> config) : mRenderConfig(config)
{
	mNumPasses = 0;
	mGbufferPass = std::make_shared<GbufferPassParser>();
	mDeferredPass = std::make_shared<DeferredPassParser>();
	mShaderConfig = std::make_shared<ShaderConfig>();
}

void ShaderManager::processShadersInFolder(std::string path)
{
	//TODO : raise error if gbuffer or deferred shaders are missing.
	std::string vsFile = path + "/gbuffer.vert";
	std::string fsFile = path + "/gbuffer.frag";
	mGbufferPass->loadGLSLFiles(vsFile, fsFile);
	mPassIndex[mGbufferPass] = mNumPasses++;

	vsFile = path + "/deferred.vert";
	fsFile = path + "/deferred.frag";
	mDeferredPass->loadGLSLFiles(vsFile, fsFile);
	mPassIndex[mDeferredPass] = mNumPasses++;

	int numCompositePasses = 0;
	for (const auto& entry : fs::directory_iterator(path)) {
		std::string filename = entry.path().filename().string();
		if (filename.substr(0, 9) == "composite" && filename.substr(filename.length() - 5, 5) == ".frag")
			numCompositePasses++;
	}

	mCompositePasses.resize(numCompositePasses);
	vsFile = path + "/composite.vert";
	for (int i = 0; i < numCompositePasses; i++) {
		fsFile = path + "/composite" + std::to_string(i) + ".frag";
		mCompositePasses[i] = std::make_shared<CompositePassParser>();
		mCompositePasses[i]->loadGLSLFiles(vsFile, fsFile);
		mPassIndex[mCompositePasses[i]] = mNumPasses++;
	}
	populateShaderConfig();
	prepareTextureOperationTable();
}

void ShaderManager::populateShaderConfig() {
	mShaderConfig->materialPipeline = (ShaderConfig::MaterialPipeline) mGbufferPass->mMaterialType;
	mShaderConfig->vertexLayout = mGbufferPass->getVertexInputLayout();
	mShaderConfig->objectBufferLayout = mGbufferPass->getObjectBufferLayout();
	mShaderConfig->sceneBufferLayout = mDeferredPass->getSceneBufferLayout();
	mShaderConfig->cameraBufferLayout = mGbufferPass->getCameraBufferLayout();
}

void ShaderManager::prepareTextureOperationTable() {
	//process gbuffer out textures:
	for (auto& elem : mGbufferPass->getTextureOutputLayout()->elements) {
		std::string texName = getOutTextureName(elem.second.name);
		mTextureOperationTable[texName] = std::vector<TextureOperation>(mNumPasses, TextureOperation::eTextureNoOp);
		mTextureOperationTable[texName][mPassIndex[mGbufferPass]] = TextureOperation::eTextureWrite;
	}

	// process input textures of deferred pass:
	for (auto& elem : mDeferredPass->getCombinedSamplerLayout()->elements) {
		std::string texName = getInTextureName(elem.second.name);
		if (texName == "Depth") {
			//TODO : validate presence of depth output in gbuffer pass
		}
		else if(texName == "Shadow"){
			//TODO : validate presence of depth output in shadow pass
		}
		if(mTextureOperationTable.find(texName) != mTextureOperationTable.end())// must be render target of previous pass
			mTextureOperationTable[texName][mPassIndex[mDeferredPass]] = TextureOperation::eTextureRead;
	}
	//process out textures of deferred paas:
	for (auto& elem : mDeferredPass->getTextureOutputLayout()->elements) {
		std::string texName = getOutTextureName(elem.second.name);
		if (mTextureOperationTable.find(texName) == mTextureOperationTable.end())
			mTextureOperationTable[texName] = std::vector<TextureOperation>(mNumPasses, TextureOperation::eTextureNoOp);
		mTextureOperationTable[texName][mPassIndex[mDeferredPass]] = TextureOperation::eTextureWrite;
	}

	for(int i = 0; i < mCompositePasses.size(); i++){
		auto compositePass = mCompositePasses[i];
		// process input textures of composite pass:
		for (auto& elem : compositePass->getCombinedSamplerLayout()->elements) {
			std::string texName = getInTextureName(elem.second.name);
			if (mTextureOperationTable.find(texName) != mTextureOperationTable.end())// must be render target of previous passes
				mTextureOperationTable[texName][mPassIndex[mCompositePasses[i]]] = TextureOperation::eTextureRead;
		}
		//add composite out texture to the set:
		for (auto& elem : compositePass->getTextureOutputLayout()->elements) {
			std::string texName = getOutTextureName(elem.second.name);
			if (mTextureOperationTable.find(texName) == mTextureOperationTable.end())
				mTextureOperationTable[texName] = std::vector<TextureOperation>(mNumPasses, TextureOperation::eTextureNoOp);
			mTextureOperationTable[texName][mPassIndex[mCompositePasses[i]]] = TextureOperation::eTextureWrite;
		}
	}
}
TextureOperation ShaderManager::getNextOperation(std::string texName, std::shared_ptr<BaseParser> pass) {
	for (int i = mPassIndex[pass] + 1; i < mNumPasses; i++) {
		TextureOperation op = mTextureOperationTable[texName][i];
		if (op != TextureOperation::eTextureNoOp)
			return op;
	}
	return TextureOperation::eTextureNoOp;
}

TextureOperation ShaderManager::getPrevOperation(std::string texName, std::shared_ptr<BaseParser> pass) {
	for (int i = mPassIndex[pass] - 1; i >= 0; i--) {
		TextureOperation op = mTextureOperationTable[texName][i];
		if (op != TextureOperation::eTextureNoOp)
			return op;
	}
	return TextureOperation::eTextureNoOp;
}

std::unordered_map<std::string, std::pair<vk::ImageLayout, vk::ImageLayout>> ShaderManager::getTextureLayouts(std::shared_ptr<BaseParser> pass, std::shared_ptr<OutputDataLayout> outputLayout) {
	std::unordered_map<std::string, std::pair<vk::ImageLayout, vk::ImageLayout>> layouts;
	for (auto& elem : outputLayout->elements) {
		std::string texName = getOutTextureName(elem.second.name);

		//compute initial layout:
		TextureOperation prevOp = getPrevOperation(texName, pass);
		if (prevOp == TextureOperation::eTextureNoOp) {
			layouts[texName].first = vk::ImageLayout::eUndefined;
		}
		else {// this layout would have been set by prev paas
			layouts[texName].first = vk::ImageLayout::eColorAttachmentOptimal;
		}

		//compute final layout:
		TextureOperation nextOp = getNextOperation(texName, pass);
		switch (nextOp)
		{
			case TextureOperation::eTextureNoOp:// TODO : What should be the correct layout if the RT is never used again?
			case TextureOperation::eTextureRead:
				layouts[texName].second = vk::ImageLayout::eShaderReadOnlyOptimal;
				break;
			case TextureOperation::eTextureWrite:
				layouts[texName].second = vk::ImageLayout::eColorAttachmentOptimal;
				break;
			default:
				break;
		}
	}
	return layouts;
}

std::vector<vk::Pipeline> ShaderManager::createPipelines(vk::Device device, vk::CullModeFlags cullMode, vk::FrontFace frontFace,
															int numDirectionalLights, int numPointLights){
	mPipelines.resize(mNumPasses);
	mPipelines[0] = mGbufferPass->createGraphicsPipeline(device, mRenderConfig->renderTargetFormat, mRenderConfig->depthFormat,
		cullMode, frontFace, getTextureLayouts(mGbufferPass, mGbufferPass->getTextureOutputLayout()));
	mPipelines[1] = mDeferredPass->createGraphicsPipeline(device, mRenderConfig->renderTargetFormat, mRenderConfig->depthFormat,
		getTextureLayouts(mDeferredPass, mDeferredPass->getTextureOutputLayout()),
		numDirectionalLights, numPointLights);
	int i = 2;
	for (auto pass : mCompositePasses) {
		mPipelines[i] = mCompositePasses[i]->createGraphicsPipeline(device, mRenderConfig->renderTargetFormat, mRenderConfig->depthFormat,
			getTextureLayouts(mDeferredPass, mDeferredPass->getTextureOutputLayout()));
	}
	return mPipelines;
}

std::vector<vk::PipelineLayout> ShaderManager::getPipelinesLayouts(){
	std::vector<vk::PipelineLayout> layouts(mNumPasses);
	layouts[0] = mGbufferPass->getPipelineLayout();
	layouts[1] = mDeferredPass->getPipelineLayout();
	int i = 2;
	for (auto pass : mCompositePasses) {
		layouts[i++] = mCompositePasses[i]->getPipelineLayout();
	}
	return layouts;
}


} // namespace shader
} // namespace svulkan2