/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>

#include "dds/dds.hpp"

#include "switch.hpp"

using namespace org::eclipse::cyclonedds;
using namespace dds::sub::qos;

// republisher topic
int main(int argc, char **argv)
{
    if(argc < 3) {
        std::cerr << "Supply republishing type and sample amount." << std::endl;
        return -1;
    }

    try {
        unsigned long num_samps = std::stoul(argv[2]);
        if (num_samps == 0 || num_samps > 200000000) {
            std::cerr << "Number of samples outside bounds." << std::endl;
            return -2;
        }

        dds::domain::DomainParticipant participant(domain::default_id());

        /*dds::topic::qos::TopicQos qos;
        qos << dds::core::policy::Reliability(dds::core::policy::ReliabilityKind::RELIABLE, dds::core::Duration(8, 8))
            << dds::core::policy::History(dds::core::policy::HistoryKind::KEEP_LAST, 5)
            << dds::core::policy::DestinationOrder(dds::core::policy::DestinationOrderKind::BY_SOURCE_TIMESTAMP)
            << dds::core::policy::TypeConsistencyEnforcement(dds::core::policy::TypeConsistencyKind::ALLOW_TYPE_COERCION, true, true, true, false, false)
            << dds::core::policy::DataRepresentation({dds::core::policy::DataRepresentationId::XCDR2})
            ;
        //qos is not passed to topics made using this participant???
        participant.default_topic_qos(qos);*/

        dds::sub::Subscriber subscriber(participant);

        return validation::validate(participant, subscriber, num_samps, std::string(argv[1])) ? 0 : -2;
    } catch (const dds::core::Exception& e) {
        std::cerr << "DDS exception thrown: " << e.what() << std::endl;
        return -3;
    } catch (const std::exception& e) {
        std::cerr << "C++ exception thrown: " << e.what() << std::endl;
        return -4;
    } catch (...) {
        std::cerr << "Unknown exception thrown" << std::endl;
        return -5;
    }

    return 0;
}
