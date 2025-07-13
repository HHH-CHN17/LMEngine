#ifndef OFXVEC3F
#define OFXVEC3F

#include <cmath>

class ofVec3f {
public:

    static const int DIM = 3;
    float x;
    float y;
    float z;


    ofVec3f();
    explicit ofVec3f( float scalar );
    ofVec3f( float x, float y, float z=0 );

    float * getPtr() {
        return (float*)&x;
    }
    const float * getPtr() const {
        return (const float *)&x;
    }

    float& operator[]( int n ){
        return getPtr()[n];
    }

    float operator[]( int n ) const {
        return getPtr()[n];
    }

    ofVec3f  operator+( const ofVec3f& pnt ) const;

    ofVec3f& operator+=( const ofVec3f& pnt );

    ofVec3f  operator-( const ofVec3f& vec ) const;

    ofVec3f& operator-=( const ofVec3f& vec );

    ofVec3f  operator-() const;

    ofVec3f  operator*( const ofVec3f& vec ) const;

    ofVec3f& operator*=( const ofVec3f& vec );

    ofVec3f& operator*=( const float f );

    ofVec3f  operator*( const float f ) const;

    ofVec3f  operator/( const ofVec3f& vec ) const;

    ofVec3f& operator/=( const ofVec3f& vec );

    ofVec3f  operator/( const float f ) const;

    ofVec3f& operator/=( const float f );

    bool operator==( const ofVec3f& vec ) const;

    bool operator!=( const ofVec3f& vec ) const;

    bool match( const ofVec3f& vec, float tolerance = 0.0001f ) const;

    float length() const;

    float lengthSquared() const;

};

inline ofVec3f::ofVec3f(): x(0), y(0), z(0) {}
inline ofVec3f::ofVec3f( float _all ): x(_all), y(_all), z(_all) {}
inline ofVec3f::ofVec3f( float _x, float _y, float _z ):x(_x), y(_y), z(_z) {}


inline ofVec3f ofVec3f::operator+( const ofVec3f& pnt ) const {
    return ofVec3f( x+pnt.x, y+pnt.y, z+pnt.z );
}

inline ofVec3f& ofVec3f::operator+=( const ofVec3f& pnt ) {
    x+=pnt.x;
    y+=pnt.y;
    z+=pnt.z;
    return *this;
}

inline ofVec3f ofVec3f::operator-( const ofVec3f& vec ) const {
    return ofVec3f( x-vec.x, y-vec.y, z-vec.z );
}


inline ofVec3f& ofVec3f::operator-=( const ofVec3f& vec ) {
    x -= vec.x;
    y -= vec.y;
    z -= vec.z;
    return *this;
}

inline ofVec3f ofVec3f::operator-() const {
    return ofVec3f( -x, -y, -z );
}



inline ofVec3f ofVec3f::operator*( const ofVec3f& vec ) const {
    return ofVec3f( x*vec.x, y*vec.y, z*vec.z );
}

inline ofVec3f& ofVec3f::operator*=( const ofVec3f& vec ) {
    x*=vec.x;
    y*=vec.y;
    z*=vec.z;
    return *this;
}

inline ofVec3f& ofVec3f::operator*=( const float f ) {
    x*=f;
    y*=f;
    z*=f;
    return *this;
}

inline ofVec3f ofVec3f::operator*( const float f ) const {
    return ofVec3f( x*f, y*f, z*f );
}



inline ofVec3f ofVec3f::operator/( const ofVec3f& vec ) const {
    return ofVec3f( vec.x!=0 ? x/vec.x : x , vec.y!=0 ? y/vec.y : y, vec.z!=0 ? z/vec.z : z );
}

inline ofVec3f& ofVec3f::operator/=( const ofVec3f& vec ) {
    vec.x!=0 ? x/=vec.x : x;
    vec.y!=0 ? y/=vec.y : y;
    vec.z!=0 ? z/=vec.z : z;
    return *this;
}


inline ofVec3f& ofVec3f::operator/=( const float f ) {
    if(f == 0) return *this;

    x/=f;
    y/=f;
    z/=f;
    return *this;
}

inline ofVec3f ofVec3f::operator/( const float f ) const {
    if(f == 0) return ofVec3f( x, y, z);

    return ofVec3f( x/f, y/f, z/f );
}

inline bool ofVec3f::operator==( const ofVec3f& vec ) const {
    return (x == vec.x) && (y == vec.y) && (z == vec.z);
}

inline bool ofVec3f::operator!=( const ofVec3f& vec ) const {
    return (x != vec.x) || (y != vec.y) || (z != vec.z);
}

inline bool ofVec3f::match( const ofVec3f& vec, float tolerance ) const{
    return (fabs(x - vec.x) < tolerance)
    && (fabs(y - vec.y) < tolerance)
    && (fabs(z - vec.z) < tolerance);
}

inline float ofVec3f::length() const {
    return (float)sqrt( x*x + y*y + z*z );
}

inline float ofVec3f::lengthSquared() const {
    return (float)(x*x + y*y + z*z);
}


#endif
// OFXVEC3F

