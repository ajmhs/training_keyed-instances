/*
* (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

#include <iostream>
#include <string>
#include <dds/pub/ddspub.hpp>
#include <rti/util/util.hpp>      // for sleep()
#include <rti/config/Logger.hpp>  // for logging
// alternatively, to include all the standard APIs:
//  <dds/dds.hpp>
// or to include both the standard APIs and extensions:
//  <rti/rti.hpp>
//
// For more information about the headers and namespaces, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSNamespaceModule.html
// For information on how to use extensions, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSCpp2Conventions.html

#include "application.hpp"  // for command line parsing and ctrl-c
#include "shapes.hpp"
#include <cmath>

void run_publisher_application(unsigned int domain_id, unsigned int sample_count, const std::string& color)
{
    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id);

    // Create a Topic with a name and a datatype
    dds::topic::Topic< ::ShapeTypeExtended> topic(participant, "Square");

    // Create a Publisher
    dds::pub::Publisher publisher(participant);

    // Create a DataWriter with default QoS
    dds::pub::DataWriter< ::ShapeTypeExtended> writer(publisher, topic);

    ::ShapeTypeExtended data;

    // Tell Connext that we will be modifying a particular instance
    dds::core::InstanceHandle instance_handle = writer.register_instance(data);

    const int left = 15, top = 15, right = 248, bottom = 278; // limits
    const int shape_size = 30;
    int x = left-shape_size, y = bottom - top / 2;
    const float AMPLITUDE = 100.0f;
    const float FREQUENCY = 0.0475f;

    data.color(color);
    data.shapesize(shape_size);
    data.fillKind(ShapeFillKind::SOLID_FILL);
    
    // Main loop, write data
    unsigned int samples_written = 0;
    for (; !application::shutdown_requested && samples_written < sample_count; ++samples_written) {

        if (++x > right)
          x = left-shape_size;

        y = (int)(bottom - top) / 2 + AMPLITUDE * std::sin(FREQUENCY * x);
        
        data.x(x);
        data.y(y);

        std::cout << "Writing a " << color << " square at (" << x << "," << y << "), count: " << samples_written << std::endl;
        
        writer.write(data);

        rti::util::sleep(dds::core::Duration(1));
    }

    // de-register instance
    writer.dispose_instance(instance_handle);
}

int main(int argc, char *argv[])
{

    using namespace application;

    // Parse arguments and handle control-C
    auto arguments = parse_arguments(argc, argv);
    if (arguments.parse_result == ParseReturn::exit) {
        return EXIT_SUCCESS;
    } else if (arguments.parse_result == ParseReturn::failure) {
        return EXIT_FAILURE;
    }
    setup_signal_handlers();

    // Sets Connext verbosity to help debugging
    rti::config::Logger::instance().verbosity(arguments.verbosity);

    try {
        run_publisher_application(arguments.domain_id, arguments.sample_count, arguments.color);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_publisher_application(): " << ex.what()
        << std::endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    return EXIT_SUCCESS;
}
