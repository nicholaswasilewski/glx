#include "platform.h"
#include "matrixMath.cpp"

#include <stdlib.h>
#include <string.h>
#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>

#define PI 3.14159

struct camera
{
    //Projection Stuff
    float FOV;
    float Aspect;
    float Near;
    float Far;
    
    //View stuff
    v3 Position;
    v3 Forward;
    v3 Up;
};

struct game_data
{
    bool Initialized;

    //A model?
    uint32 VertexBufferID;
    uint32 ColorBufferID;
    uint32 UVBufferID;
    uint32 ProgramID;
    uint32 MatrixID;

    uint32 TextureID;

    float BoxRotation;

    camera Camera;
};

void CameraStrafeLeft(float dT, float speed, camera* Camera)
{
    Camera->Position = Camera->Position - speed*dT*Normalize(Cross(Camera->Forward, Camera->Up));
}

void CameraStrafeRight(float dT, float speed, camera* Camera)
{
    Camera->Position = Camera->Position + speed*dT*Normalize(Cross(Camera->Forward, Camera->Up));   
}

void CameraWalkForward(float dT, float speed, camera* Camera)
{
    Camera->Position = Camera->Position + speed*dT*Camera->Forward;
}

void CameraWalkBackward(float dT, float speed, camera* Camera)   
{
    Camera->Position = Camera->Position - speed*dT*Camera->Forward;
}

void RotateCamera(camera* Camera, float dX, float dY, float CameraSpeed)
{
    {
	mat3 XRot = MakeRotation3x3(Camera->Up, PI*dX*CameraSpeed);
	v3 NewForward = XRot*Camera->Forward;
	Camera->Forward = NewForward;
    }

    {
	mat3 YRot = MakeRotation3x3(Cross(Camera->Forward, Camera->Up), PI*dY*CameraSpeed);
	v3 NewForward = YRot*Camera->Forward;
	v3 NewUp = YRot*Camera->Up;
	Camera->Forward = NewForward;
	Camera->Up = NewUp;
    }
}

void GLErrorShow()
{
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR)
    {
	char* msg;
	if (error == GL_INVALID_OPERATION)
	{
	    msg = "Invalid Operation";
	}
	else if (error == GL_INVALID_ENUM)
	{
	    msg = "Invalid enum";
	}
	else if (error == GL_INVALID_VALUE)
	{
	    msg = "Invalid value";
	}
	else if (error == GL_OUT_OF_MEMORY)
	{
	    msg = "Out of memory";
	}
	else if (error == GL_INVALID_FRAMEBUFFER_OPERATION)
	{
	    msg = "Invalid framebuffer operation";
	}
	printf("OpenGL error: %d - %s\n", error, msg);
    }
}

mat4 GenerateCameraPerspective(camera Camera)
{
    mat4 Projection = MakePerspectiveProjection(Camera.FOV, Camera.Aspect, Camera.Near, Camera.Far);
    return Projection;
}

mat4 GenerateCameraView(camera Camera)
{
    PrintVector(Camera.Up);
    mat4 View = DirectionView(Camera.Position, Camera.Forward, Camera.Up);
    return View;
}

GLuint LoadDDS(const char * filePath)
{
    int8 header[124];

    FILE *fp = fopen(filePath, "rb");
    if (fp == 0)
    {
	printf("File not found: %s", filePath);
	return 0;
    }

    char fileCode[4];
    fread(fileCode, 1, 4, fp);
    if (strncmp(fileCode, "DDS ", 4) != 0)
    {
	fclose(fp);
	printf("File is not DDS: %s", filePath);
	return 0;
    }

    fread(&header, 124, 1, fp);

    uint32 height = *(uint32*)&(header[8]);
    uint32 width = *(uint32*)&(header[12]);
    uint32 linearSize = *(uint32*)&(header[16]);
    uint32 mipMapCount = *(uint32*)&(header[24]);
    char fourCC[5];
    fourCC[0] = header[80];
    fourCC[1] = header[81];
    fourCC[2] = header[82];
    fourCC[3] = header[83];
    fourCC[4] = '\0';
    
    uint32 bufferSize = mipMapCount > 1 ? linearSize * 2 : linearSize;
    uint8* buffer = (uint8*)malloc(bufferSize * sizeof(uint8));
    fread(buffer, 1, bufferSize, fp);
    fclose(fp);

//    uint32 components;
    uint32 format;
    
    format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    if (strncmp(fourCC, "DXT1", 4) == 0)
    {
	format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
//	components = 3;
    }
    else if (strncmp (fourCC, "DXT3", 4) == 0)
    {
	format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
//	components = 4;
    }
    else if (strncmp(fourCC, "DXT5", 4) == 0)
    {
	format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
//	components = 4;
    }
    else
    {
	free(buffer);
	printf("File not DXT compressed: %s", filePath);
	return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    uint32 blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    uint32 offset = 0;
    for(uint32 level = 0; level < mipMapCount && (width || height); ++level)
    {
	uint32 size = ((width+3)/4)*((height+3)/4)*blockSize;
	glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, buffer + offset);
	offset += size;
	width /= 2;
	height /= 2;
    }

    free(buffer);
    return textureID;
}
GLuint LoadBMP(char* filePath)
{
    uint8 header[54];
    uint32 dataPos;
    uint32 width, height;
    uint32 imageSize;
    char* data;

    FILE * file = fopen(filePath, "rb");
    if (!file)
    {
	printf("File not found: %s\n", filePath);
	return 0;
    }

    if (fread(header, 1, 54, file) != 54) {
	printf("Malformed BMP: %s\n", filePath);
	return 0;
    }

    if (header[0] != 'B' || header[1]!= 'M')
    {
	printf("Malformed BMP: %s\n", filePath);
	return 0;
    }

    dataPos = *(int32*)&(header[0x0a]);
    imageSize = *(int32*)&(header[0x22]);
    width = *(int32*)&(header[0x12]);
    height = *(int32*)&(header[0x16]);

    if (imageSize==0)
    {
	imageSize = width*height*3;
    }
    if (dataPos==0)
    {
	dataPos=54;
    }

    data = (char*)malloc(imageSize*sizeof(char));
    fread(data, 1, imageSize, file);
    fclose(file);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    free(data);
    return textureID;
}

GLuint LoadShaders(char* vertexShaderFilePath, char* fragmentShaderFilePath)
{
    GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    FILE* vertexShaderFile = fopen(vertexShaderFilePath, "r");
    fseek(vertexShaderFile, 0L, SEEK_END);
    int32 vertexShaderFileLength = ftell(vertexShaderFile);
    rewind(vertexShaderFile);
 
    char* vertexShaderCode = (char*)malloc(vertexShaderFileLength+1);
    fread(vertexShaderCode, 1, vertexShaderFileLength+1, vertexShaderFile);
    vertexShaderCode[vertexShaderFileLength] = '\0';
    fclose(vertexShaderFile);

    GLint result = GL_FALSE;
    int32 infoLogLength;
    
    glShaderSource(vertexShaderID, 1, &vertexShaderCode, 0);
    glCompileShader(vertexShaderID);
    free(vertexShaderCode);

    glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
	char* error = (char*)malloc(infoLogLength);
	glGetShaderInfoLog(vertexShaderID, infoLogLength, 0, error);
	printf("%s error:\n%s\n", vertexShaderFilePath, error);
	free(error);
    }
    char* fragmentShaderCode = 0;
    FILE* fragmentShaderFile = fopen(fragmentShaderFilePath, "r");
    fseek(fragmentShaderFile, 0L, SEEK_END);
    int fragmentShaderFileLength = ftell(fragmentShaderFile);
    rewind(fragmentShaderFile);

    fragmentShaderCode = (char*)malloc(fragmentShaderFileLength+1);
    fread(fragmentShaderCode, 1, fragmentShaderFileLength+1, fragmentShaderFile);
    fragmentShaderCode[fragmentShaderFileLength] = '\0';
    fclose(fragmentShaderFile);

    glShaderSource(fragmentShaderID, 1, &fragmentShaderCode, 0);
    glCompileShader(fragmentShaderID);
    free(fragmentShaderCode);

    glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
	char* error = (char*)malloc(infoLogLength+1);
	glGetShaderInfoLog(fragmentShaderID, infoLogLength, 0, error);
	printf("%s error:\n%s\n", fragmentShaderFilePath, error);
	free(error);
    }

    GLuint programID = glCreateProgram();
    glAttachShader(programID, vertexShaderID);
    glAttachShader(programID, fragmentShaderID);
    glLinkProgram(programID);

    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
	char* error = (char*)malloc(infoLogLength+1);
	glGetProgramInfoLog(programID, infoLogLength, 0, error);
	printf("%s\n", error);
	free(error);
    }

    glDetachShader(programID, vertexShaderID);
    glDetachShader(programID, fragmentShaderID);
    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return programID;
}

void UpdateAndRender(platform_data* Platform)
{    
    game_data* Game = (game_data*)(((char*)Platform->MainMemory)+0);
//    input *LastInput = Platform->LastInput;
    input *Input = Platform->NewInput;
//    controller OldKeyboard = LastInput->Keyboard;
    controller Keyboard = Input->Keyboard;
    
    if (!Game->Initialized)
    {
	glClearColor(0.0, 0.0, 0.4, 0.0);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	GLfloat vertexBufferData[] = {
	    -1.0f,-1.0f,-1.0f,
	    -1.0f,-1.0f, 1.0f,
	    -1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f,-1.0f, 
	    -1.0f,-1.0f,-1.0f,
	    -1.0f, 1.0f,-1.0f,
	    1.0f,-1.0f, 1.0f,
	    -1.0f,-1.0f,-1.0f,
	    1.0f,-1.0f,-1.0f,
	    1.0f, 1.0f,-1.0f,
	    1.0f,-1.0f,-1.0f,
	    -1.0f,-1.0f,-1.0f,
	    -1.0f,-1.0f,-1.0f,
	    -1.0f, 1.0f, 1.0f,
	    -1.0f, 1.0f,-1.0f,
	    1.0f,-1.0f, 1.0f,
	    -1.0f,-1.0f, 1.0f,
	    -1.0f,-1.0f,-1.0f,
	    -1.0f, 1.0f, 1.0f,
	    -1.0f,-1.0f, 1.0f,
	    1.0f,-1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f,-1.0f,-1.0f,
	    1.0f, 1.0f,-1.0f,
	    1.0f,-1.0f,-1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f,-1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f,-1.0f,
	    -1.0f, 1.0f,-1.0f,
	    1.0f, 1.0f, 1.0f,
	    -1.0f, 1.0f,-1.0f,
	    -1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    -1.0f, 1.0f, 1.0f,
	    1.0f,-1.0f, 1.0f
	};

	GLfloat colorBufferData[] = {
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 1.0f,
	};
	
	static const GLfloat uvBufferData[] = {
	    0.000059f, 0.000004f,
	    0.000103f, 0.336048f,
	    0.335973f, 0.335903f,
	    1.000023f, 0.000013f,
	    0.667979f, 0.335851f,
	    0.999958f, 0.336064f,
	    0.667979f, 0.335851f,
	    0.336024f, 0.671877f,
	    0.667969f, 0.671889f,
	    1.000023f, 0.000013f,
	    0.668104f, 0.000013f,
	    0.667979f, 0.335851f,
	    0.000059f, 0.000004f,
	    0.335973f, 0.335903f,
	    0.336098f, 0.000071f,
	    0.667979f, 0.335851f,
	    0.335973f, 0.335903f,
	    0.336024f, 0.671877f,
	    1.000004f, 0.671847f,
	    0.999958f, 0.336064f,
	    0.667979f, 0.335851f,
	    0.668104f, 0.000013f,
	    0.335973f, 0.335903f,
	    0.667979f, 0.335851f,
	    0.335973f, 0.335903f,
	    0.668104f, 0.000013f,
	    0.336098f, 0.000071f,
	    0.000103f, 0.336048f,
	    0.000004f, 0.671870f,
	    0.336024f, 0.671877f,
	    0.000103f, 0.336048f,
	    0.336024f, 0.671877f,
	    0.335973f, 0.335903f,
	    0.667969f, 0.671889f,
	    1.000004f, 0.671847f,
	    0.667979f, 0.335851f
	};

	glGenBuffers(1, &Game->VertexBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, Game->VertexBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexBufferData), vertexBufferData, GL_STATIC_DRAW);
	
	glGenBuffers(1, &Game->ColorBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, Game->ColorBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colorBufferData), colorBufferData, GL_STATIC_DRAW);

	glGenBuffers(1, &Game->UVBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, Game->UVBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvBufferData), uvBufferData, GL_STATIC_DRAW);
	    
	Game->ProgramID = LoadShaders("textureShader.vert", "textureShader.frag");
	Game->MatrixID = glGetUniformLocation(Game->ProgramID, "MVP");
	Game->TextureID = LoadDDS("uvtemplate.dds");

	camera Camera = {0};
	Camera.FOV = PI*0.25f;
	Camera.Aspect = 800.0f/600.0f;
	Camera.Near = 0.01f;
	Camera.Far = 1000.0f;

	Camera.Position = V3(0.0f, 5.0f, 5.0f);
	Camera.Forward = V3(0, -1.0f,-1.0f);
	Camera.Up = V3(0,1,0);
	
	Game->Camera = Camera;
	Game->Initialized = true;
    }

    if (Keyboard.Left.Down)
    {
	CameraStrafeLeft(Input->dT, 3.0f, &Game->Camera);
    }
    else if (Keyboard.Right.Down)	
    {
	CameraStrafeRight(Input->dT, 3.0f, &Game->Camera);
    }

    if (Keyboard.Up.Down)
    {
	CameraWalkForward(Input->dT, 3.0f, &Game->Camera);
    }
    else if (Keyboard.Down.Down)
    {
	CameraWalkBackward(Input->dT, 3.0f, &Game->Camera);
    }
    

    RotateCamera(&Game->Camera,
		 Keyboard.RStick.X / 100.0f,
		 Keyboard.RStick.Y / 100.0f,
		 Input->dT*1.0f);

    if (Keyboard.RStick.X != 0.0f || Keyboard.RStick.Y != 0.0f) {
    }

    Game->BoxRotation += PI*(1.0f/120.0f);

    mat4 Projection = GenerateCameraPerspective(Game->Camera);
    mat4 View = GenerateCameraView(Game->Camera);

    mat4 Model = Identity4x4();
    v3 ModelAxis = V3(0.25f, 1.0f, .5f);
    mat4 Rotation = MakeRotation(ModelAxis, Game->BoxRotation);
    mat4 Scale = MakeScale(V3(1.0f, 1.0f, 1.0f));
    mat4  Translation = MakeTranslation(V3(0.0f, 0.0f, 0.0f));
    Model = Translation * Rotation * Scale;
    mat4 MVP = Projection * View * Model;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	    
    glUseProgram(Game->ProgramID);
    glUniformMatrix4fv(Game->MatrixID, 1, GL_FALSE, &MVP.E[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Game->TextureID);
    glUniform1i(Game->TextureID, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, Game->VertexBufferID);
    glVertexAttribPointer(0,
			  3,
			  GL_FLOAT,
			  GL_FALSE,
			  0,
			  (void*)0
	);
    /*
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, Game->ColorBufferID;)
    glVertexAttribPointer(0,
			  3,
			  GL_FLOAT,
			  GL_FALSE,
			  0,
			  (void*)0
	);
    */
    
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, Game->UVBufferID);
    glVertexAttribPointer(1,
			  2,
			  GL_FLOAT,
			  GL_FALSE,
			  0,
			  (void*)0
			  );
    
    glDrawArrays(GL_TRIANGLES, 0, 12*3);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}
