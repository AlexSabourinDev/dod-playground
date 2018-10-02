#include "game.h"
#include <vector>
#include <string>
#include <math.h>
#include <assert.h>

const int kObjectCount = 1000000;
const int kAvoidCount = 20;



static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }
static float RandomFloat(int from, int to) { return RandomFloat01() * ((float)to - (float)from) + (float)from; }


// -------------------------------------------------------------------------------------------------
// components we use in our "game". these are all just simple structs with some data.


// 2D position: just x,y coordinates
struct PositionComponent
{
    float x, y;
};


// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent
{
    uint8_t colorR, colorG, colorB;
    uint8_t spriteIndex;
};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent
{
    int xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent
{
    float velx, vely;

    void Initialize(float minSpeed, float maxSpeed)
    {
        // random angle
        float angle = RandomFloat01() * 3.1415926f * 2;
        // random movement speed between given min & max
        float speed = RandomFloat(minSpeed, maxSpeed);
        // velocity x & y components
        velx = cosf(angle) * speed;
        vely = sinf(angle) * speed;
    }
};


// -------------------------------------------------------------------------------------------------
// super simple "game entities system", using struct-of-arrays data layout.
// we just have an array for each possible component, and a flags array bit bits indicating
// which components are "present".

// "ID" of a game object is just an index into the scene array.
typedef size_t EntityID;

struct Entities
{
    static constexpr WorldBoundsComponent s_WorldBounds = { -128, 128, -64, 64 };

    // arrays of data; the sizes of all of them are the same. EntityID (just an index)
    // is used to access data for any "object/entity". The "object" itself is nothing
    // more than just an index into these arrays.
    
    // data for all components
    std::vector<PositionComponent> m_Positions;
    std::vector<SpriteComponent> m_Sprites;
    std::vector<MoveComponent> m_Moves;

	EntityID m_IdGenerator = 0;
    
    void reserve(size_t n)
    {
        m_Positions.resize(n);
        m_Sprites.resize(n);
        m_Moves.resize(n);
    }
    
	EntityID AddEntity()
	{
		return m_IdGenerator++;
	}
};


// The "scene"
static Entities s_Objects;


// -------------------------------------------------------------------------------------------------
// "systems" that we have; they operate on components of game objects


struct MoveSystem
{
    __declspec(noinline) void UpdateSystem(double time, float deltaTime)
    {
		// Cache pointer for debug performance
		PositionComponent* positionsData = s_Objects.m_Positions.data();
		MoveComponent* movementsData = s_Objects.m_Moves.data();

        // go through all the objects
        for (size_t io = 0, no = kObjectCount + kAvoidCount; io != no; ++io)
        {
            PositionComponent& pos = positionsData[io];
            MoveComponent& move = movementsData[io];
            
            // update position based on movement velocity & delta time
            pos.x += move.velx * deltaTime;
            pos.y += move.vely * deltaTime;
            
            // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
            if (pos.x < Entities::s_WorldBounds.xMin)
            {
                move.velx = -move.velx;
                pos.x = (float)Entities::s_WorldBounds.xMin;
            }
            if (pos.x > Entities::s_WorldBounds.xMax)
            {
                move.velx = -move.velx;
                pos.x = (float)Entities::s_WorldBounds.xMax;
            }
            if (pos.y < Entities::s_WorldBounds.yMin)
            {
                move.vely = -move.vely;
                pos.y = (float)Entities::s_WorldBounds.yMin;
            }
			if (pos.y > Entities::s_WorldBounds.yMax)
            {
                move.vely = -move.vely;
                pos.y = (float)Entities::s_WorldBounds.yMax;
            }
        }
    }
};

static MoveSystem s_MoveSystem;


// "Avoidance system" works out interactions between objects that "avoid" and "should be avoided".
// Objects that avoid:
// - when they get closer to things that should be avoided than the given distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidanceSystem
{
    static constexpr float kAvoidDistance = 1.3f;

    void UpdateSystem(double time, float deltaTime)
    {
		// Cache pointer for debug performance
		PositionComponent* positionsData = s_Objects.m_Positions.data();
		SpriteComponent* spritesData = s_Objects.m_Sprites.data();
		MoveComponent* movesData = s_Objects.m_Moves.data();

        constexpr unsigned int kCellCount = 64;
        constexpr unsigned int kShiftAmount = 6;
        constexpr unsigned int kGridCellSize[2] = { (Entities::s_WorldBounds.xMax - Entities::s_WorldBounds.xMin) / kCellCount, (Entities::s_WorldBounds.yMax - Entities::s_WorldBounds.yMin) / kCellCount };

        static EntityID avoidEntityGrid[kCellCount * kCellCount][kAvoidCount];
        unsigned int activeAvoidCount[kCellCount * kCellCount] = { 0 };

        for (size_t ia = kObjectCount, na = kObjectCount + kAvoidCount; ia != na; ++ia)
        {
            const PositionComponent& avoidposition = positionsData[ia];

            unsigned int topRight[2] = { (unsigned int)(avoidposition.x + kAvoidDistance - (float)Entities::s_WorldBounds.xMin) / kGridCellSize[0], (unsigned int)(avoidposition.y + kAvoidDistance - (float)Entities::s_WorldBounds.yMin) / kGridCellSize[1] };
            topRight[0] = topRight[0] - ((topRight[0] & kCellCount) >> kShiftAmount);
            topRight[1] = topRight[1] - ((topRight[1] & kCellCount) >> kShiftAmount);

            unsigned int bottomLeft[2] = { (unsigned int)(avoidposition.x - kAvoidDistance - (float)Entities::s_WorldBounds.xMin) / kGridCellSize[0], (unsigned int)(avoidposition.y - kAvoidDistance - (float)Entities::s_WorldBounds.yMin) / kGridCellSize[1] };
            bottomLeft[0] = bottomLeft[0] - ((bottomLeft[0] & kCellCount) >> kShiftAmount);
            bottomLeft[1] = bottomLeft[1] - ((bottomLeft[1] & kCellCount) >> kShiftAmount);

            for (unsigned int y = bottomLeft[1]; y <= topRight[1]; ++y)
            {
                for (unsigned int x = bottomLeft[0]; x <= topRight[0]; ++x)
                {
                    unsigned int hash = x << kShiftAmount | y;
                    avoidEntityGrid[hash][activeAvoidCount[hash]] = ia;
                    ++activeAvoidCount[hash];
                }
            }
        }

        // go through all the objects
        for (size_t io = 0, no = kObjectCount; io != no; ++io)
        {
            PositionComponent& myposition = positionsData[io];

            unsigned int x = (unsigned int)(myposition.x - (float)Entities::s_WorldBounds.xMin) / kGridCellSize[0];
            x = x - ((x & kCellCount) >> kShiftAmount);

            unsigned int y = (unsigned int)(myposition.y - (float)Entities::s_WorldBounds.yMin) / kGridCellSize[1];
            x = y - ((y & kCellCount) >> kShiftAmount);

            unsigned int hash = x << kShiftAmount | y;

            // check each thing in avoid list
            for (unsigned int ia = 0, na = activeAvoidCount[hash]; ia != na; ++ia)
            {
                EntityID avoid = avoidEntityGrid[hash][ia];
                const PositionComponent& avoidposition = positionsData[avoid];

                float dx = avoidposition.x - myposition.x;
                float dy = avoidposition.y - myposition.y;
                // is our position closer to "thing to avoid" position than the avoid distance?
                if ((dx * dx + dy * dy) - kAvoidDistance * kAvoidDistance < 0.0f)
                {
					MoveComponent& move = movesData[io];

					// flip velocity
					move.velx = -move.velx;
					move.vely = -move.vely;

					// move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
					myposition.x += move.velx * deltaTime * 1.1f;
					myposition.y += move.vely * deltaTime * 1.1f;
                    
                    // also make our sprite take the color of the thing we just bumped into
                    SpriteComponent& avoidSprite = spritesData[avoid];
                    SpriteComponent& mySprite = spritesData[io];
                    mySprite.colorR = avoidSprite.colorR;
                    mySprite.colorG = avoidSprite.colorG;
                    mySprite.colorB = avoidSprite.colorB;
                }
            }
        }
    }
};

static AvoidanceSystem s_AvoidanceSystem;


// -------------------------------------------------------------------------------------------------
// "the game"


extern "C" void game_initialize(void)
{
    s_Objects.reserve(kObjectCount + kAvoidCount);

	// Cache for debug performance
	PositionComponent* positionsData = s_Objects.m_Positions.data();
	SpriteComponent* spritesData = s_Objects.m_Sprites.data();
	MoveComponent* movesData = s_Objects.m_Moves.data();

    // create regular objects that move
    for (auto i = 0; i < kObjectCount; ++i)
    {
        EntityID go = s_Objects.AddEntity();

        // position it within world bounds
        positionsData[go].x = RandomFloat(Entities::s_WorldBounds.xMin, Entities::s_WorldBounds.xMax);
        positionsData[go].y = RandomFloat(Entities::s_WorldBounds.yMin, Entities::s_WorldBounds.yMax);

        // setup a sprite for it (random sprite index from first 5), and initial white color
        spritesData[go].colorR = 255;
        spritesData[go].colorG = 255;
        spritesData[go].colorB = 255;
        spritesData[go].spriteIndex = (uint8_t)(255 * RandomFloat(0.0f, 1.0f));

        // make it move
        s_Objects.m_Moves[go].Initialize(0.5f, 0.7f);
    }

    // create objects that should be avoided
    for (auto i = 0; i < kAvoidCount; ++i)
    {
        EntityID go = s_Objects.AddEntity();
        
        // position it in small area near center of world bounds
        positionsData[go].x = RandomFloat(Entities::s_WorldBounds.xMin, Entities::s_WorldBounds.xMax) * 0.2f;
        positionsData[go].y = RandomFloat(Entities::s_WorldBounds.yMin, Entities::s_WorldBounds.yMax) * 0.2f;

        // setup a sprite for it (6th one), and a random color
        spritesData[go].colorR = (uint8_t)(255 * RandomFloat(0.5f, 1.0f));
        spritesData[go].colorG = (uint8_t)(255 * RandomFloat(0.5f, 1.0f));
        spritesData[go].colorB = (uint8_t)(255 * RandomFloat(0.5f, 1.0f));
        spritesData[go].spriteIndex = 255;
        
        // make it move, slowly
        movesData[go].Initialize(0.1f, 0.2f);
    }
}


extern "C" void game_destroy(void)
{
}


extern "C" int game_update(sprite_pos_data_t* posData, sprite_sprite_data_t* spriteData, double time, float deltaTime)
{
    int objectCount = 0;
    
    // update object systems
    s_MoveSystem.UpdateSystem(time, deltaTime);
    s_AvoidanceSystem.UpdateSystem(time, deltaTime);

    // go through all objects
    memcpy(posData, s_Objects.m_Positions.data(), sizeof(PositionComponent) * s_Objects.m_Positions.size());
    memcpy(spriteData, s_Objects.m_Sprites.data(), sizeof(SpriteComponent) * s_Objects.m_Sprites.size());
    return (int)s_Objects.m_Positions.size();
}

