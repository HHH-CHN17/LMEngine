#ifndef OFVEC2F
#define OFVEC2F

class ofVec2f {
public:

    static const int DIM = 2;

    float x;
    float y;

    ofVec2f();
    ofVec2f( float x, float y );

    void set( float x, float y );

    void set( const ofVec2f& vec );

   float length() const;

    bool operator==( const ofVec2f& vec ) const;

    bool operator!=( const ofVec2f& vec ) const;

    bool match( const ofVec2f& vec, float tolerance = 0.0001f ) const;

    ofVec2f& operator+=( const ofVec2f& vec );

    ofVec2f& operator+=( const float f );

    ofVec2f  operator+( const ofVec2f& vec ) const;

    ofVec2f  operator+( const float f ) const;


    ofVec2f& operator-=( const ofVec2f& vec );

    ofVec2f& operator-=( const float f );

    ofVec2f  operator-( const ofVec2f& vec ) const;

    ofVec2f  operator-() const;


    ofVec2f  operator*( const ofVec2f& vec ) const;

    ofVec2f  operator*( const float f ) const;

    ofVec2f& operator*=( const ofVec2f& vec );

    ofVec2f& operator*=( const float f );

    ofVec2f  operator/( const ofVec2f& vec ) const;


    ofVec2f  operator/( const float f ) const;

    ofVec2f& operator/=( const ofVec2f& vec );

    ofVec2f& operator/=( const float f );


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

};

inline ofVec2f::ofVec2f(): x(0), y(0) {}
inline ofVec2f::ofVec2f( float _x, float _y ):x(_x), y(_y) {}



inline void ofVec2f::set( float _x, float _y ) {
    x = _x;
    y = _y;
}

inline void ofVec2f::set( const ofVec2f& vec ) {
    x = vec.x;
    y = vec.y;
}


inline bool ofVec2f::operator==( const ofVec2f& vec ) const {
    return (x == vec.x) && (y == vec.y);
}

inline bool ofVec2f::operator!=( const ofVec2f& vec ) const {
    return (x != vec.x) || (y != vec.y);
}

inline bool ofVec2f::match( const ofVec2f& vec, float tolerance ) const {
    return (fabs(x - vec.x) < tolerance)
    && (fabs(y - vec.y) < tolerance);
}



inline ofVec2f ofVec2f::operator+( const ofVec2f& vec ) const {
    return ofVec2f( x+vec.x, y+vec.y);
}

inline ofVec2f& ofVec2f::operator+=( const ofVec2f& vec ) {
    x += vec.x;
    y += vec.y;
    return *this;
}
inline ofVec2f ofVec2f::operator+( const float f ) const {
    return ofVec2f( x+f, y+f);
}

inline ofVec2f& ofVec2f::operator+=( const float f ) {
    x += f;
    y += f;
    return *this;
}


inline ofVec2f& ofVec2f::operator-=( const float f ) {
    x -= f;
    y -= f;
    return *this;
}

inline ofVec2f& ofVec2f::operator-=( const ofVec2f& vec ) {
    x -= vec.x;
    y -= vec.y;
    return *this;
}


inline ofVec2f ofVec2f::operator-( const ofVec2f& vec ) const {
    return ofVec2f(x-vec.x, y-vec.y);
}


inline ofVec2f ofVec2f::operator-() const {
    return ofVec2f(-x, -y);
}


inline ofVec2f ofVec2f::operator*( const ofVec2f& vec ) const {
    return ofVec2f(x*vec.x, y*vec.y);
}

inline ofVec2f& ofVec2f::operator*=( const ofVec2f& vec ) {
    x*=vec.x;
    y*=vec.y;
    return *this;
}

inline ofVec2f ofVec2f::operator*( const float f ) const {
    return ofVec2f(x*f, y*f);
}

inline ofVec2f& ofVec2f::operator*=( const float f ) {
    x*=f;
    y*=f;
    return *this;
}


inline ofVec2f ofVec2f::operator/( const ofVec2f& vec ) const {
    return ofVec2f( vec.x!=0 ? x/vec.x : x , vec.y!=0 ? y/vec.y : y);
}

inline ofVec2f& ofVec2f::operator/=( const ofVec2f& vec ) {
    vec.x!=0 ? x/=vec.x : x;
    vec.y!=0 ? y/=vec.y : y;
    return *this;
}

inline ofVec2f ofVec2f::operator/( const float f ) const {
    if(f == 0) return ofVec2f(x, y);

    return ofVec2f(x/f, y/f);
}

inline ofVec2f& ofVec2f::operator/=( const float f ) {
    if(f == 0) return *this;

    x/=f;
    y/=f;
    return *this;
}

inline float ofVec2f::length() const {
    return (float)sqrt( x*x + y*y);
}

#endif // OFVEC2F

