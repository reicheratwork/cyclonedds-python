"""
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
"""

from typing import Optional, Union, TYPE_CHECKING

from .internal import c_call, dds_c_t
from .core import Entity, DDSException, Listener
from .domain import DomainParticipant
from .topic import Topic
from .qos import _CQos, Qos, LimitedScopeQos, PublisherQos, DataWriterQos

from cyclonedds._clayer import ddspy_write, ddspy_write_ts, ddspy_dispose, ddspy_writedispose, ddspy_writedispose_ts, \
    ddspy_dispose_handle, ddspy_dispose_handle_ts, ddspy_register_instance, ddspy_unregister_instance,   \
    ddspy_unregister_instance_handle, ddspy_unregister_instance_ts, ddspy_unregister_instance_handle_ts, \
    ddspy_lookup_instance, ddspy_dispose_ts


if TYPE_CHECKING:
    import cyclonedds


class Publisher(Entity):
    def __init__(
            self,
            domain_participant: DomainParticipant,
            qos: Optional[Qos] = None,
            listener: Optional[Listener] = None):
        if not isinstance(domain_participant, DomainParticipant):
            raise TypeError(f"{domain_participant} is not a cyclonedds.domain.DomainParticipant.")

        if qos is not None:
            if isinstance(qos, LimitedScopeQos) and not isinstance(qos, PublisherQos):
                raise TypeError(f"{qos} is not appropriate for a Publisher")
            elif not isinstance(qos, Qos):
                raise TypeError(f"{qos} is not a valid qos object")

        if listener is not None:
            if not isinstance(listener, Listener):
                raise TypeError(f"{listener} is not a valid listener object.")

        cqos = _CQos.qos_to_cqos(qos) if qos else None
        try:
            super().__init__(
                self._create_publisher(
                    domain_participant._ref,
                    cqos,
                    listener._ref if listener else None
                ),
                listener=listener
            )
        finally:
            if cqos:
                _CQos.cqos_destroy(cqos)

        self._keepalive_entities = [self.participant]

    def suspend(self):
        ret = self._suspend(self._ref)
        if ret == 0:
            return
        raise DDSException(ret, f"Occurred while suspending {repr(self)}")

    def resume(self):
        ret = self._resume(self._ref)
        if ret == 0:
            return
        raise DDSException(ret, f"Occurred while resuming {repr(self)}")

    def wait_for_acks(self, timeout: int):
        ret = self._wait_for_acks(self._ref, timeout)
        if ret == 0:
            return True
        elif ret == DDSException.DDS_RETCODE_TIMEOUT:
            return False
        raise DDSException(ret, f"Occurred while waiting for acks from {repr(self)}")

    @c_call("dds_create_publisher")
    def _create_publisher(self, domain_participant: dds_c_t.entity, qos: dds_c_t.qos_p,
                          listener: dds_c_t.listener_p) -> dds_c_t.entity:
        pass

    @c_call("dds_suspend")
    def _suspend(self, publisher: dds_c_t.entity) -> dds_c_t.returnv:
        pass

    @c_call("dds_resume")
    def _resume(self, publisher: dds_c_t.entity) -> dds_c_t.returnv:
        pass

    @c_call("dds_wait_for_acks")
    def _wait_for_acks(self, publisher: dds_c_t.entity, timeout: dds_c_t.duration) -> dds_c_t.returnv:
        pass


class DataWriter(Entity):
    def __init__(self,
                 publisher_or_participant: Union[DomainParticipant, Publisher],
                 topic: Topic,
                 qos: Optional[Qos] = None,
                 listener: Optional[Listener] = None):
        if not isinstance(publisher_or_participant, (DomainParticipant, Publisher)):
            raise TypeError(f"{publisher_or_participant} is not a cyclonedds.domain.DomainParticipant"
                            " or cyclonedds.pub.Publisher.")

        if not isinstance(topic, Topic):
            raise TypeError(f"{topic} is not a cyclonedds.topic.Topic.")

        if qos is not None:
            if isinstance(qos, LimitedScopeQos) and not isinstance(qos, DataWriterQos):
                raise TypeError(f"{qos} is not appropriate for a DataWriter")
            elif not isinstance(qos, Qos):
                raise TypeError(f"{qos} is not a valid qos object")

        cqos = _CQos.qos_to_cqos(qos) if qos else None
        try:
            super().__init__(
                self._create_writer(
                    publisher_or_participant._ref,
                    topic._ref,
                    cqos,
                    listener._ref if listener else None
                ),
                listener=listener
            )
        finally:
            if cqos:
                _CQos.cqos_destroy(cqos)

        self._topic = topic
        self.data_type = topic.data_type
        self._keepalive_entities = [self.publisher, self.topic]

    @property
    def topic(self) -> 'cyclonedds.topic.Topic':
        return self._topic

    def write(self, sample, timestamp=None):
        if not isinstance(sample, self.data_type):
            raise TypeError(f"{sample} is not of type {self.data_type}")

        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        if timestamp is not None:
            ret = ddspy_write_ts(self._ref, ser, timestamp)
        else:
            ret = ddspy_write(self._ref, ser)

        if ret < 0:
            raise DDSException(ret, f"Occurred while writing sample in {repr(self)}")

    def write_dispose(self, sample, timestamp=None):
        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        if timestamp is not None:
            ret = ddspy_writedispose_ts(self._ref, ser, timestamp)
        else:
            ret = ddspy_writedispose(self._ref, ser)

        if ret < 0:
            raise DDSException(ret, f"Occurred while writedisposing sample in {repr(self)}")

    def dispose(self, sample, timestamp=None):
        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        if timestamp is not None:
            ret = ddspy_dispose_ts(self._ref, ser, timestamp)
        else:
            ret = ddspy_dispose(self._ref, ser)

        if ret < 0:
            raise DDSException(ret, f"Occurred while disposing in {repr(self)}")

    def dispose_instance_handle(self, handle, timestamp=None):
        if timestamp is not None:
            ret = ddspy_dispose_handle_ts(self._ref, handle, timestamp)
        else:
            ret = ddspy_dispose_handle(self._ref, handle)

        if ret < 0:
            raise DDSException(ret, f"Occurred while disposing in {repr(self)}")

    def register_instance(self, sample):
        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        ret = ddspy_register_instance(self._ref, ser)
        if ret < 0:
            raise DDSException(ret, f"Occurred while registering instance in {repr(self)}")
        return ret

    def unregister_instance(self, sample, timestamp: int = None):
        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        if timestamp is not None:
            ret = ddspy_unregister_instance_ts(self._ref, ser, timestamp)
        else:
            ret = ddspy_unregister_instance(self._ref, ser)

        if ret < 0:
            raise DDSException(ret, f"Occurred while unregistering instance in {repr(self)}")

    def unregister_instance_handle(self, handle, timestamp: int = None):
        if timestamp is not None:
            ret = ddspy_unregister_instance_handle_ts(self._ref, handle, timestamp)
        else:
            ret = ddspy_unregister_instance_handle(self._ref, handle)

        if ret < 0:
            raise DDSException(ret, f"Occurred while unregistering instance handle n {repr(self)}")

    def wait_for_acks(self, timeout: int):
        ret = self._wait_for_acks(self._ref, timeout)
        if ret == 0:
            return True
        elif ret == Exception.DDS_RETCODE_TIMEOUT:
            return False
        raise DDSException(ret, f"Occurred while waiting for acks from {repr(self)}")

    def lookup_instance(self, sample):
        ser = sample.serialize()
        ser += b'\0' * (len(ser) % 4)

        ret = ddspy_lookup_instance(self._ref, ser)
        if ret < 0:
            raise DDSException(ret, f"Occurred while lookup up instance from {repr(self)}")
        if ret == 0:
            return None
        return ret

    @c_call("dds_create_writer")
    def _create_writer(self, publisher: dds_c_t.entity, topic: dds_c_t.entity, qos: dds_c_t.qos_p,
                       listener: dds_c_t.listener_p) -> dds_c_t.entity:
        pass

    @c_call("dds_wait_for_acks")
    def _wait_for_acks(self, publisher: dds_c_t.entity, timeout: dds_c_t.duration) -> dds_c_t.returnv:
        pass
