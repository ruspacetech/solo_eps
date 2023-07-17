#include <ItcLogger/Logger.hpp>

#include <solo_eps_data_point.hpp>

#include <solo_eps_42_data_provider.hpp>

namespace Nos3
{
    REGISTER_DATA_PROVIDER(Solo_eps42DataProvider,"SOLO_EPS_42_PROVIDER");

    extern ItcLogger::Logger *sim_logger;

    Solo_eps42DataProvider::Solo_eps42DataProvider(const boost::property_tree::ptree& config) : SimData42SocketProvider(config)
    {
        sim_logger->trace("Solo_eps42DataProvider::Solo_eps42DataProvider:  Constructor executed");

        connect_reader_thread_as_42_socket_client(
            config.get("simulator.hardware-model.data-provider.hostname", "localhost"),
            config.get("simulator.hardware-model.data-provider.port", 4242) );

        _sc = config.get("simulator.hardware-model.data-provider.spacecraft", 0);
    }

    boost::shared_ptr<SimIDataPoint> Solo_eps42DataProvider::get_data_point(void) const
    {
        sim_logger->trace("Solo_eps42DataProvider::get_data_point:  Executed");

        // Get the 42 data
        const boost::shared_ptr<Sim42DataPoint> dp42 =
            boost::dynamic_pointer_cast<Sim42DataPoint>(SimData42SocketProvider::get_data_point());

        // vvv Prepare the specific data... this may need changed in your use case
        SimIDataPoint *dp = new Solo_epsDataPoint(_sc, dp42);

        return boost::shared_ptr<SimIDataPoint>(dp);
    }
}
