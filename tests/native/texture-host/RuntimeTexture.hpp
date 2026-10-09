#pragma once
#include <cstdint>
#include <string>
class RuntimeTexture {
public:
    virtual ~RuntimeTexture() = default;
    int get_Width() { return Width; }
    int get_Height() { return Height; }
    void set_Width(int value) { Width = value; }
    void set_Height(int value) { Height = value; }
    std::string get_Id() { return Id; }
    void set_Id(std::string value) { Id = value; }
    void Dispose() {}
private:
    int Width = 0, Height = 0;
    std::string Id;
};
