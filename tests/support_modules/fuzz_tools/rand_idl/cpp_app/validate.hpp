#ifndef VALIDATION_IMPL
#define VALIDATION_IMPL

#include "dds/dds.hpp"
#include <string>
#include <iostream>
#include <thread>
#include "org/eclipse/cyclonedds/topic/datatopic.hpp"


namespace validation {

    void print_hex(ddsi_keyhash_t &in)
    {
        static const char *hex = "0123456789ABCDEF";
        std::string buffer;
        buffer.resize(32);

        for(size_t i = 0; i < 16; ++i) {
            buffer[i*2] = hex[(in.value[i]>>4) & 0xF];
            buffer[i*2+1] = hex[in.value[i] & 0xF];
        }

        std::cout << buffer << std::endl;
    }

    template<class T>
    bool validate_impl( dds::domain::DomainParticipant &participant,
                        dds::sub::Subscriber &subscriber,
                        unsigned long n_samples,
                        const std::string &topic_name)
    {
        /*create topic*/
        dds::topic::Topic<T> topic(participant, topic_name);

        dds::sub::qos::DataReaderQos qos;
        qos << dds::core::policy::Reliability(dds::core::policy::ReliabilityKind::RELIABLE, dds::core::Duration(8, 8))
            << dds::core::policy::History(dds::core::policy::HistoryKind::KEEP_LAST, 5)
            << dds::core::policy::DestinationOrder(dds::core::policy::DestinationOrderKind::BY_SOURCE_TIMESTAMP)
            << dds::core::policy::TypeConsistencyEnforcement(dds::core::policy::TypeConsistencyKind::ALLOW_TYPE_COERCION, true, true, true, false, false)
            << dds::core::policy::DataRepresentation({dds::core::policy::DataRepresentationId::XCDR2});

        /*create reader*/
        dds::sub::DataReader<T> reader(subscriber, topic, qos);

        unsigned long seqq = 0;
        while (seqq < n_samples) {
            for (auto &s:reader.take()) {

                if (!s->info().valid())
                    return false;

                ddsi_keyhash_t m_key;
                memset(m_key.value, 0x0, 16);
                to_key<T>(s->data(), m_key);

                print_hex(m_key);

                seqq++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        return true;
    }

} //validation

#endif //VALIDATION_IMPL
