#ifndef NOS3_SOLO_EPSDATAPOINT_HPP
#define NOS3_SOLO_EPSDATAPOINT_HPP

#include <boost/shared_ptr.hpp>

#include <sim_42data_point.hpp>

namespace Nos3
{
    // vvv This is pretty standard for a data point (if one is needed to transfer data between a data provider and a hardware model for your sim)
    class Solo_epsDataPoint : public SimIDataPoint
    {
    public:
        // Constructor
        Solo_epsDataPoint(int16_t spacecraft, const boost::shared_ptr<Sim42DataPoint> dp);

        // Accessors
        std::string to_string(void) const;
        // vvv These provide the hardware model a way to get the specific data out of the data point that it will need to send
        // out bytes over its peripheral bus
        double      get_solo_eps_data_x(void) const {return _solo_eps_data[0];}
        double      get_solo_eps_data_y(void) const {return _solo_eps_data[1];}
        double      get_solo_eps_data_z(void) const {return _solo_eps_data[2];}
        bool        is_solo_eps_data_valid(void) const {return _solo_eps_data_is_valid;}
    
    private:
        // Disallow the big 4
        Solo_epsDataPoint(void) {};
        Solo_epsDataPoint(const Solo_epsDataPoint&) {};
        Solo_epsDataPoint& operator=(const Solo_epsDataPoint&) {};
        ~Solo_epsDataPoint(void) {};

        // vvv This would be the specific data you need to get from the data provider to the hardware model so that
        // the hardware model can send out bytes over its peripheral bus... but you only get to this data through the accessors above
        mutable bool   _solo_eps_data_is_valid;
        mutable double _solo_eps_data[3];
    };
}

#endif
