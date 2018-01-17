#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "CinderImGui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class LTCAreaLightApp : public App {
  public:
	class AreaLight {
	  public:
		AreaLight &glsl( gl::GlslProgRef meshGlsl ) { mMeshGlsl = meshGlsl; mMeshBatch = gl::Batch::create( geom::Plane(), mMeshGlsl ); return *this; }
		AreaLight &color( Colorf color ) { mLightColor = color; return *this; }
		AreaLight &intensity( float intensity ) { mIntensity = intensity; return *this; }
		AreaLight &transform( mat4 transform ) { mTransform = transform; return *this; }
		AreaLight &doubleSided( bool doubleSided ) { mDoubleSided = doubleSided; return *this; }
		
		Colorf getColor() const { return mLightColor; }
		float getIntensity() const { return mIntensity; }
		bool getDoubleSided() const { return mDoubleSided; }
		
		void draw() const {
			if( mMeshGlsl ) {
				updateUniforms();
				gl::ScopedFaceCulling scoledFaceCulling( ! mDoubleSided );
				gl::ScopedModelMatrix scopedModelMatrix;
				gl::multModelMatrix( mTransform );
				mMeshBatch->draw();
			}
		}
		
		vector<vec3> transformedVertices() const {
			static const vector<vec3> vertices = { vec3( 1, 0, 1 ), vec3( 1, 0, -1 ), vec3( -1, 0, -1 ), vec3( -1, 0, 1 ) };
			vector<vec3> worldVertices;
			std::transform( vertices.begin(), vertices.end(), back_inserter( worldVertices ), [&] ( const vec3 &v ) {
				return vec3( mTransform * vec4( v, 1.0 ) );
			} );
			return worldVertices;
		}
		
		Colorf mLightColor = Colorf( 1, 1, 1 );
		float mIntensity = 1.0f;
		bool mDoubleSided = false;
		
	  private:
		gl::BatchRef mMeshBatch;
		gl::GlslProgRef mMeshGlsl;
		mat4 mTransform = mat4( 1 );
		
		void updateUniforms() const {
			mMeshGlsl->uniform( "uLightColor", mLightColor );
			mMeshGlsl->uniform( "uIntensity", mIntensity );
		}
	};
	
	struct PBRMaterial {
	  public:
		PBRMaterial() {}
		
		PBRMaterial &baseColor( Colorf color ) { mBaseColor = color; return *this; }
		PBRMaterial &metalness( float metalness ) { mMetalness = metalness; return *this; }
		PBRMaterial &roughness( float roughness ) { mRoughness = roughness; return *this; }
		PBRMaterial &f0( float f0 ) { mF0 = f0; return *this; }
		
		void updateUniforms( const gl::GlslProgRef &glsl ) const {
			if( glsl ) {
				glsl->uniform( "uBaseColor", mBaseColor );
				glsl->uniform( "uMetalness", mMetalness );
				glsl->uniform( "uRoughness", mRoughness );
				glsl->uniform( "uF0", mF0 );
			}
		}
		
		Colorf mBaseColor = Colorf( 1, 1, 1 );
		float mMetalness = 0.0;
		float mRoughness = 0.0;
		float mF0 = 1.0;
	};
	
	void setup() override;
	void update() override;
	void draw() override;
	void userInterface();
	
	gl::GlslProgRef mForwardAreaLightGlsl;
	gl::BatchRef mFloorBatch;
	vector<AreaLight> mAreaLights;
	PBRMaterial mFloorMaterial;
	
	gl::Texture2dRef mLTCMatTexture;
	gl::Texture2dRef mLTCAmpTexture;
	
	CameraPersp mCamera;
	CameraUi mCameraUi;
};

void LTCAreaLightApp::setup()
{
	gl::GlslProgRef areaLightMeshGlsl;
	try {
		mForwardAreaLightGlsl = gl::GlslProg::create( loadAsset( "shaders/areaLightBRDF.vert" ), loadAsset( "shaders/areaLightBRDF.frag" ) );
		mForwardAreaLightGlsl->uniform( "uTexLTCMat", 0 );
		mForwardAreaLightGlsl->uniform( "uTexLTCAmp", 1 );
		
		areaLightMeshGlsl = gl::GlslProg::create( loadAsset( "shaders/areaLightMesh.vert" ), loadAsset( "shaders/areaLightMesh.frag" ) );
	} catch( gl::GlslProgExc &e ) {
		CI_LOG_F( "Could not compile shader: " + std::string( e.what() ) );
		exit( 0 );
	}
	
	mFloorBatch = gl::Batch::create( geom::Plane() >> geom::Scale( 10.0f ), mForwardAreaLightGlsl );
	mFloorMaterial = PBRMaterial().baseColor( Colorf( 1, 0.9, 0.8 ) ).metalness( 0 ).roughness( 0.3 ).f0( 0.9 );
	mAreaLights.emplace_back( AreaLight().glsl( areaLightMeshGlsl ).color( Colorf( 0, 1, 1 ) ).transform(
		glm::translate( vec3( 0, 1.5, 0 ) ) *
		glm::rotate( glm::radians( 90.0f ), vec3( 1, 0, 0 ) ) *
		glm::scale( vec3( 0.5, 1, 1 ) ) ) );
	
	auto ampTexFormat = gl::Texture2d::Format().minFilter( GL_LINEAR ).magFilter( GL_LINEAR ).wrap( GL_CLAMP_TO_EDGE ).internalFormat( GL_RG32F );
	auto matTexFormat = gl::Texture2d::Format().minFilter( GL_LINEAR ).magFilter( GL_LINEAR ).wrap( GL_CLAMP_TO_EDGE ).internalFormat( GL_RGBA32F );
	mLTCAmpTexture = gl::Texture2d::createFromDds( loadAsset( "textures/ltc_amp_64.dds" ), ampTexFormat );
	mLTCMatTexture = gl::Texture2d::createFromDds( loadAsset( "textures/ltc_mat_64.dds" ), matTexFormat );
//	mLTCAmpTexture = gl::Texture2d::create( loadImage( loadAsset( "textures/ltc_amp_64.png" ) ), ampTexFormat );
//	mLTCMatTexture = gl::Texture2d::create( loadImage( loadAsset( "textures/ltc_mat_64.png" ) ), matTexFormat );
	
	mCamera.setPerspective( 90.0f, getWindowAspectRatio(), 0.1f, 1000.0f );
	mCamera.lookAt( vec3( 5.0, 5.0, 2.5 ), vec3( 0 ) );
	mCameraUi.connect( getWindow(), -1 );
	mCameraUi.setCamera( &mCamera );
	
	ui::initialize();
	ui::SetWindowFontScale( 1.5f );
}

void LTCAreaLightApp::update()
{
	for( const AreaLight &light : mAreaLights ) {
		auto lightVertices = light.transformedVertices();
		mForwardAreaLightGlsl->uniform( "uLightVertices", lightVertices.data(), lightVertices.size() );
		mForwardAreaLightGlsl->uniform( "uLightColor", light.getColor() );
		mForwardAreaLightGlsl->uniform( "uLightIntensity", light.getIntensity() );
		mForwardAreaLightGlsl->uniform( "uDoubleSided", light.getDoubleSided() );
	}
	mForwardAreaLightGlsl->uniform( "uCameraPos", mCamera.getEyePoint() );
}

void LTCAreaLightApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	
	gl::ScopedMatrices scopedMatrices;
	gl::ScopedModelMatrix scopedModelMatrix;
	gl::setMatrices( mCamera );
	
	gl::ScopedDepth scopedDepth( true );
	gl::ScopedFaceCulling scopedCulling( true );
	
	gl::ScopedTextureBind scopedTexture0( mLTCMatTexture, 0 );
	gl::ScopedTextureBind scopedTexture1( mLTCAmpTexture, 1 );
	
	mFloorMaterial.updateUniforms( mFloorBatch->getGlslProg() );
	mFloorBatch->draw();
	
	for( const AreaLight &light : mAreaLights )
		light.draw();
	
	userInterface();
}

void LTCAreaLightApp::userInterface() {
	ui::ColorEdit3( "Base Color", reinterpret_cast<float*>( &mFloorMaterial.mBaseColor ) );
	ui::SliderFloat( "Metalness", &mFloorMaterial.mMetalness, 0.0f, 1.0f );
	ui::SliderFloat( "Roughness", &mFloorMaterial.mRoughness, 0.0f, 1.0f );
	ui::SliderFloat( "F0", &mFloorMaterial.mF0, 0.0f, 1.0f );
	
	ui::Separator();
	ui::Spacing();
	ui::Spacing();
	
	ui::ColorEdit3( "Light Color", reinterpret_cast<float*>( &mAreaLights.front().mLightColor ) );
	ui::SliderFloat( "Light Intensity", &mAreaLights.front().mIntensity, 0.0f, 2.0f );
	ui::Checkbox( "Double Sided", &mAreaLights.front().mDoubleSided );
}

CINDER_APP( LTCAreaLightApp, RendererGl( RendererGl::Options().msaa( 8 ) ), [] ( App::Settings *settings ) {
	settings->setWindowSize( 1200, 600 );
	settings->setHighDensityDisplayEnabled();
} )
