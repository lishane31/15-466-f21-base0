#include "ColorTextureProgram.hpp"

#include "Mode.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <list>
#include <deque>

/**
 * Different types of blocks
 */
enum BLOCK_TYPE {
    regular = 1,
    split = 2,
    del = 3,
    leftScore = 4,
    rightScore = 5,
    shrink = 6,
    expand = 7
};

/**
 * Used to make special blocks
 */
struct Block {
public:
    glm::vec2 pos;
    BLOCK_TYPE type;
    Block(glm::vec2 pos, BLOCK_TYPE type): pos(pos), type(type) {}
};

/*
 * PongMode is a game mode that implements a single-player game of Pong.
 */
struct PongMode : Mode {
	PongMode();
	virtual ~PongMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	glm::vec2 court_radius = glm::vec2(7.0f, 5.0f);
	glm::vec2 paddle_radius = glm::vec2(0.2f, 1.0f);
    glm::vec2 block_radius = glm::vec2(0.2f, 0.2f);
	glm::vec2 ball_radius = glm::vec2(0.2f, 0.2f);

	glm::vec2 left_paddle = glm::vec2(-court_radius.x + 0.5f, 0.0f);
	glm::vec2 right_paddle = glm::vec2( court_radius.x - 0.5f, 0.0f);

	std::vector<glm::vec2> balls;
	std::vector<glm::vec2> ball_velocities;

	uint32_t left_score = 0;
	uint32_t right_score = 0;

	float ai_offset = 0.0f;
	float ai_offset_update = 0.0f;

    std::vector<Block> blocks;
    float block_spawn = 3.0f;
    float block_update = 0.0f;

	//----- pretty gradient trails -----

	float trail_length = 1.3f;
	std::vector<std::deque< glm::vec3 >> ball_trails; //stores (x,y,age), oldest elements first

	//----- opengl assets / helpers ------

	//draw functions will work on vectors of vertices, defined as follows:
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
			Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 1*4 + 4*2, "PongMode::Vertex should be packed");

	//Shader program that draws transformed, vertices tinted with vertex colors:
	ColorTextureProgram color_texture_program;

	//Buffer used to hold vertex data during drawing:
	GLuint vertex_buffer = 0;

	//Vertex Array Object that maps buffer locations to color_texture_program attribute locations:
	GLuint vertex_buffer_for_color_texture_program = 0;

	//Solid white texture:
	GLuint white_tex = 0;

	//matrix that maps from clip coordinates to court-space coordinates:
	glm::mat3x2 clip_to_court = glm::mat3x2(1.0f);
	// computed in draw() as the inverse of OBJECT_TO_CLIP
	// (stored here so that the mouse handling code can use it to position the paddle)


    //----- Methods for handling block interactions -----
    
    /**
     * Called if the block is destroyed, 
     * perform the special block effect based on type 
     */
    void do_effect(Block &block);

    /**
     * Returns the color of this block depending on the type
     */
    glm::u8vec4 get_color(Block &block);
};
