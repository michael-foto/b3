#ifndef ISYSTEM
#define ISYSTEM

class ISystem {
  public:
    ISystem() = default;
    virtual ~ISystem() = default;
    virtual void update() = 0;
};

#endif