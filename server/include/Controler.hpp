#ifndef CONTROLER_HPP
#define CONTROLER_HPP

class Controler {
public:
    Controler(const std::string& path);
    
private:
    std::vector<Client> clients
};

#endif // CONTROLER_HPP