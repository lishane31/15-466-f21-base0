#include "PongMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

PongMode::PongMode() {
    balls.emplace_back(glm::vec2(0.0f, 0.0f));
    ball_velocities.emplace_back(glm::vec2(-1.0f, 0.0f));
    ball_trails.emplace_back(std::deque< glm::vec3 >());

                    blocks.emplace_back(Block(glm::vec2(0, 0), expand));

    // balls.emplace_back(glm::vec2(0.0f, 0.0f));
    // ball_velocities.emplace_back(glm::vec2(1.0f, 0.0f));
    // ball_trails.emplace_back(std::deque< glm::vec3 >());

	//set up trail as if ball has been here for 'forever':
    for(int i = 0; i < balls.size(); i++) {
        ball_trails[i].clear();
        ball_trails[i].emplace_back(balls[i], trail_length);
        ball_trails[i].emplace_back(balls[i], 0.0f);
    }

	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PongMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

PongMode::~PongMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool PongMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
		);
		left_paddle.y = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y;
	}

	return false;
}

void PongMode::update(float elapsed) {

	static std::mt19937 mt; //mersenne twister pseudo-random number generator

	//----- paddle update -----
    {
        block_update += elapsed;
        if(block_update > block_spawn) {
            block_update -= block_spawn;
            int randType = mt() % 10;

            // float randX = (float) ((mt() % (unsigned int) court_radius.x)) - (int)(0.5f * court_radius.x);
            // float randY = (float) ((mt() % (unsigned int) court_radius.y)) - (int)(0.5f * court_radius.y);

            float randX = mt() % 8 * (court_radius.x / 5.0f) - 4.0f / 5.0f * court_radius.x;
            float randY = mt() % 8 * (court_radius.y / 5.0f) - 4.0f / 5.0f * court_radius.y;

            switch(randType) {
                case 1: 
                    blocks.emplace_back(Block(glm::vec2(randX, randY), split));
                    break;
                case 2:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), del));
                    break;
                case 3:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), leftScore));
                    break;
                case 4:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), rightScore));
                    break;
                case 5:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), shrink));
                    break;
                case 6:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), expand));
                    break;
                default:
                    blocks.emplace_back(Block(glm::vec2(randX, randY), regular));
                    break;
            }
        }
    }

	{ //right player ai:
		ai_offset_update -= elapsed;
		if (ai_offset_update < elapsed) {
			//update again in [0.5,1.0) seconds:
			ai_offset_update = (mt() / float(mt.max())) * 0.5f + 0.5f;
			ai_offset = (mt() / float(mt.max())) * 2.5f - 1.25f;
		}
		if (right_paddle.y < balls[0].y + ai_offset) {
			right_paddle.y = std::min(balls[0].y + ai_offset, right_paddle.y + 2.0f * elapsed);
		} else {
			right_paddle.y = std::max(balls[0].y + ai_offset, right_paddle.y - 2.0f * elapsed);
		}
	}

	//clamp paddles to court:
	right_paddle.y = std::max(right_paddle.y, -court_radius.y + paddle_radius.y);
	right_paddle.y = std::min(right_paddle.y,  court_radius.y - paddle_radius.y);

	left_paddle.y = std::max(left_paddle.y, -court_radius.y + paddle_radius.y);
	left_paddle.y = std::min(left_paddle.y,  court_radius.y - paddle_radius.y);

	//----- ball update -----

	//speed of ball doubles every four points:
	float speed_multiplier = 4.0f * std::pow(2.0f, (left_score + right_score) / 4.0f);

	//velocity cap, though (otherwise ball can pass through paddles):
	speed_multiplier = std::min(speed_multiplier, 10.0f);

    for(int i = 0; i < balls.size(); i++) {
	    balls[i] += elapsed * speed_multiplier * ball_velocities[i];
    }

	//---- collision handling ----
    auto obj_vs_balls = [this](glm::vec2 const &obj, glm::vec2 const &obj_radius) {
        bool hitSomething = false;
        for(int i = 0; i < balls.size(); i++) {
            //compute area of overlap:
            glm::vec2 min = glm::max(obj - obj_radius, balls[i] - ball_radius);
            glm::vec2 max = glm::min(obj + obj_radius, balls[i] + ball_radius);

            //if no overlap, no collision:
            if (min.x > max.x || min.y > max.y) continue;

            if (max.x - min.x > max.y - min.y) {
                //wider overlap in x => bounce in y direction:
                if (balls[i].y > obj.y) {
                    balls[i].y = obj.y + obj_radius.y + ball_radius.y;
                    ball_velocities[i].y = std::abs(ball_velocities[i].y);
                } else {
                    balls[i].y = obj.y - obj_radius.y - ball_radius.y;
                    ball_velocities[i].y = -std::abs(ball_velocities[i].y);
                }
            } else {
                //wider overlap in y => bounce in x direction:
                if (balls[i].x > obj.x) {
                    balls[i].x = obj.x + obj_radius.x + ball_radius.x;
                    ball_velocities[i].x = std::abs(ball_velocities[i].x);
                } else {
                    balls[i].x = obj.x - paddle_radius.x - ball_radius.x;
                    ball_velocities[i].x = -std::abs(ball_velocities[i].x);
                }
                //warp y velocity based on offset from paddle center:
                float vel = (balls[i].y - obj.y) / (obj_radius.y + ball_radius.y);
                ball_velocities[i].y = glm::mix(ball_velocities[i].y, vel, 0.75f);
            }

            hitSomething = true;
        }

        return hitSomething;
	};

	//paddles:
	obj_vs_balls(left_paddle, paddle_radius);
	obj_vs_balls(right_paddle, paddle_radius);

    //blocks:
	for(int counter = 0; counter < blocks.size(); counter++) {
        if(obj_vs_balls(blocks[counter].pos, block_radius)) {
            do_effect(blocks[counter]);

            //Shrinking or expanding removes all blocks, so break
            if(!blocks.empty()) {
                blocks.erase(blocks.begin() + counter);
                counter--;
            }
            else
                break;
        }
    }

	//court walls:
    for(int i = 0; i < balls.size(); i++) {
        if (balls[i].y > court_radius.y - ball_radius.y) {
            balls[i].y = court_radius.y - ball_radius.y;
            if (ball_velocities[i].y > 0.0f) {
                ball_velocities[i].y = -ball_velocities[i].y;
            }
        }
        if (balls[i].y < -court_radius.y + ball_radius.y) {
            balls[i].y = -court_radius.y + ball_radius.y;
            if (ball_velocities[i].y < 0.0f) {
                ball_velocities[i].y = -ball_velocities[i].y;
            }
        }

        if (balls[i].x > court_radius.x - ball_radius.x) {
            balls[i].x = court_radius.x - ball_radius.x;
            if (ball_velocities[i].x > 0.0f) {
                ball_velocities[i].x = -ball_velocities[i].x;
                left_score += 1;

                //Shrink the walls, making it harder to defend
                if(court_radius.x > 3.5f && court_radius.y > 2.5f)
                {
                    court_radius -= glm::vec2(0.7f, 0.5f);
                    paddle_radius -= glm::vec2(0.02f, 0.1f);
                    ball_radius -= glm::vec2(0.02f, 0.02f);
                    left_paddle += glm::vec2(0.7f - 0.05f, 0);
                    right_paddle -= glm::vec2(0.7f + 0.05f, 0);
                    trail_length -= 0.13f;

                    //Remove all blocks instead of re-placing them because I am bad at math
                    blocks.clear();
                }
            }
        }
        if (balls[i].x < -court_radius.x + ball_radius.x) {
            balls[i].x = -court_radius.x + ball_radius.x;
            if (ball_velocities[i].x < 0.0f) {
                ball_velocities[i].x = -ball_velocities[i].x;
                right_score += 1;

                //Shrink the walls, making it harder to defend
                if(court_radius.x > 3.5f && court_radius.y > 2.5f)
                {
                    court_radius -= glm::vec2(0.7f, 0.5f);
                    paddle_radius -= glm::vec2(0.02f, 0.1f);
                    ball_radius -= glm::vec2(0.02f, 0.02f);
                    left_paddle += glm::vec2(0.7f - 0.05f, 0);
                    right_paddle -= glm::vec2(0.7f + 0.05f, 0);
                    trail_length -= 0.13f;
                    
                    //Remove all blocks instead of re-placing them because I am bad at math
                    blocks.clear();
                }
            }
        }
    }

	//----- gradient trails -----

	//age up all locations in ball trail:
    for(int i = 0; i < balls.size(); i++) {
        for (auto &t : ball_trails[i]) {
            t.z += elapsed;
        }

        //store fresh location at back of ball trail:
        ball_trails[i].emplace_back(balls[i], 0.0f);

        //trim any too-old locations from back of trail:
        //NOTE: since trail drawing interpolates between points, only removes back element if second-to-back element is too old:
        while (ball_trails[i].size() >= 2 && ball_trails[i][1].z > trail_length) {
            ball_trails[i].pop_front();
        }
    }
}

void PongMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x193b59ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xf2d2b6ff);
    const glm::u8vec4 left_color = HEX_TO_U8VEC4(0x55ea46ee);
    const glm::u8vec4 right_color = HEX_TO_U8VEC4(0xdc143cee);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xf2ad94ff);
	const std::vector< glm::u8vec4 > trail_colors = {
		HEX_TO_U8VEC4(0xf2ad9488),
		HEX_TO_U8VEC4(0xf2897288),
		HEX_TO_U8VEC4(0xbacac088),
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	//shadows for everything (except the trail):
	// glm::vec2 s = glm::vec2(0.0f,-shadow_offset);

	// draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	// draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	// draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	// draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	// draw_rectangle(left_paddle+s, paddle_radius, shadow_color);
	// draw_rectangle(right_paddle+s, paddle_radius, shadow_color);
	// draw_rectangle(ball+s, ball_radius, shadow_color);

	//ball's trail:
    for(int i = 0; i < balls.size(); i++) {
        if (ball_trails[i].size() >= 2) {
            //start ti at second element so there is always something before it to interpolate from:
            std::deque< glm::vec3 >::iterator ti = ball_trails[i].begin() + 1;
            //draw trail from oldest-to-newest:
            constexpr uint32_t STEPS = 20;
            //draw from [STEPS, ..., 1]:
            for (uint32_t step = STEPS; step > 0; --step) {
                //time at which to draw the trail element:
                float t = step / float(STEPS) * trail_length;
                //advance ti until 'just before' t:
                while (ti != ball_trails[i].end() && ti->z > t) ++ti;
                //if we ran out of recorded tail, stop drawing:
                if (ti == ball_trails[i].end()) break;
                //interpolate between previous and current trail point to the correct time:
                glm::vec3 a = *(ti-1);
                glm::vec3 b = *(ti);
                glm::vec2 at = (t - a.z) / (b.z - a.z) * (glm::vec2(b) - glm::vec2(a)) + glm::vec2(a);

                //look up color using linear interpolation:
                //compute (continuous) index:
                float c = (step-1) / float(STEPS-1) * trail_colors.size();
                //split into an integer and fractional portion:
                int32_t ci = int32_t(std::floor(c));
                float cf = c - ci;
                //clamp to allowable range (shouldn't ever be needed but good to think about for general interpolation):
                if (ci < 0) {
                    ci = 0;
                    cf = 0.0f;
                }
                if (ci > int32_t(trail_colors.size())-2) {
                    ci = int32_t(trail_colors.size())-2;
                    cf = 1.0f;
                }
                //do the interpolation (casting to floating point vectors because glm::mix doesn't have an overload for u8 vectors):
                glm::u8vec4 color = glm::u8vec4(
                    glm::mix(glm::vec4(trail_colors[ci]), glm::vec4(trail_colors[ci+1]), cf)
                );

                //draw:
                draw_rectangle(at, ball_radius, color);
            }
        }
    }

	//solid objects:

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//paddles:
	draw_rectangle(left_paddle, paddle_radius, left_color);
	draw_rectangle(right_paddle, paddle_radius, right_color);
	

	//ball:
    for(auto ball : balls) {
	    draw_rectangle(ball, ball_radius, fg_color);
    }

    //Left blocks
    for(auto block: blocks) {
	    draw_rectangle(block.pos, ball_radius * 2.0f, get_color(block));
    }

	//scores:
	glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	for (uint32_t i = 0; i < left_score; ++i) {
		draw_rectangle(glm::vec2( -court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, left_color);
	}
	for (uint32_t i = 0; i < right_score; ++i) {
		draw_rectangle(glm::vec2( court_radius.x - (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, right_color);
	}

	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}

void PongMode::do_effect(Block &block) {
    std::cout << block.type << std::endl;
    switch(block.type) {
        case split: {
            std::vector<glm::vec2> new_balls;
            std::vector<glm::vec2> new_velocities;
            std::vector<std::deque< glm::vec3 >> new_trails;

            for(int i = 0; i < balls.size(); i++) {
                //Double every ball on screen
                new_balls.emplace_back(balls[i]);
                new_balls.emplace_back(balls[i]);

                //Add spawn a ball flying in the opposite direction
                new_velocities.emplace_back(ball_velocities[i]);
                new_velocities.emplace_back(ball_velocities[i] * glm::vec2(1, -1.0f));

                //Add new trail
                new_trails.emplace_back(ball_trails[i]);
                new_trails.emplace_back(ball_trails[i]);
            }

            balls = new_balls;
            ball_velocities = new_velocities;
            ball_trails = new_trails;
            return;
        }
        case del: {
            const auto halfSize = balls.size()/2;
            for(auto i = 0; i < halfSize; i++) {
                balls.pop_back();
                ball_velocities.pop_back();
                ball_trails.pop_back();
            }
            return;
        }
        case leftScore: {
            left_score++;
            return;
        }
        case rightScore: {
            right_score++;
            return;
        }
        case shrink: {
            court_radius = glm::vec2(3.5f, 2.5f);
            paddle_radius = glm::vec2(0.1f, 0.5f);
            ball_radius = glm::vec2(0.1f, 0.1f);
            left_paddle = glm::vec2(-3.25f, 0);
            right_paddle = glm::vec2(3.25f, 0);
            trail_length = 0.65f;
            blocks.clear();
            return;
        }
        case expand: {
            court_radius = glm::vec2(7.0f, 5.0f);
            paddle_radius = glm::vec2(0.2f, 1.0f);
            ball_radius = glm::vec2(0.2f, 0.2f);
            left_paddle = glm::vec2(-court_radius.x + 0.5f, 0.0f);
            right_paddle = glm::vec2( court_radius.x - 0.5f, 0.0f);
            trail_length = 1.3f;
            blocks.clear();
            return;
        }
        default: return;
    }
}

glm::u8vec4 PongMode::get_color(Block &block) {
    //some nice colors from the course web page:
    #define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
    switch(block.type) {
        case split: return HEX_TO_U8VEC4(0xffff00ee);
        case del: return HEX_TO_U8VEC4(0x000000ff);
        case leftScore: return HEX_TO_U8VEC4(0x55ea46ee);
        case rightScore: return HEX_TO_U8VEC4(0xdc143cee);
        case shrink: return HEX_TO_U8VEC4(0x555555ff);
        case expand: return HEX_TO_U8VEC4(0x5514eeee);
        default: return HEX_TO_U8VEC4(0xf2d2b6ff);
    }
}