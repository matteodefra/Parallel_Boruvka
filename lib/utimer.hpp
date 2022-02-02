#include <iostream>
#include <chrono>


#define START(timename) auto timename = std::chrono::system_clock::now();
#define STOP(timename,elapsed)  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - timename).count();


/**
 * @brief Class Utimer to measure time with the RAII approach (Resourse acquisition is initialization)
 * 
 */
class Utimer {
  
    using usecs = std::chrono::microseconds;
    using msecs = std::chrono::milliseconds;

    // Starting time
    std::chrono::system_clock::time_point start;
    // Finish time
    std::chrono::system_clock::time_point stop;
    // Message asssociated to the Utimer
    std::string message; 
  

    private:

        // Store the elapsed seconds
        long * us_elapsed;
    
    public:

        Utimer(const std::string m) : message(m), us_elapsed((long *)NULL) {

            start = std::chrono::system_clock::now();
        
        }
            
        Utimer(const std::string m, long * us) : message(m), us_elapsed(us) {
            
            start = std::chrono::system_clock::now();
        
        }

        ~Utimer() {

            stop = std::chrono::system_clock::now();

            std::chrono::duration<double> elapsed = stop - start;
            
            // Final duration casted to microseconds
            auto musec = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
            
            // std::cout << message << " computed in " << musec << " usec " << std::endl;
            
            // Store the elapsed time in the point us_elapsed
            if(us_elapsed != NULL) (*us_elapsed) = musec;

        }

};