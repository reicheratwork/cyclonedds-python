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

import asyncio
import concurrent.futures
from typing import AsyncGenerator, List, Optional, Union, Generator, TYPE_CHECKING

from .core import Entity, DDSException, WaitSet, ReadCondition, SampleState, InstanceState, ViewState
from .internal import c_call, dds_c_t
from .qos import _CQos
from .util import duration

from ddspy import ddspy_read, ddspy_take, ddspy_read_handle, ddspy_take_handle, ddspy_lookup_instance


if TYPE_CHECKING:
    import cyclonedds


class Subscriber(Entity):
    def __init__(
            self,
            domain_participant: 'cyclonedds.domain.DomainParticipant',
            qos: Optional['cyclonedds.core.Qos'] = None,
            listener: Optional['cyclonedds.core.Listener'] = None):
        cqos = _CQos.qos_to_cqos(qos) if qos else None
        super().__init__(
            self._create_subscriber(
                domain_participant._ref,
                cqos,
                listener._ref if listener else None
            ),
            listener=listener
        )
        if cqos:
            _CQos.cqos_destroy(cqos)

    def notify_readers(self):
        ret = self._notify_readers(self._ref)
        if ret < 0:
            raise DDSException(ret, f"Occurred while reading data in {repr(self)}")

    @c_call("dds_create_subscriber")
    def _create_subscriber(self, domain_participant: dds_c_t.entity, qos: dds_c_t.qos_p,
                           listener: dds_c_t.listener_p) -> dds_c_t.entity:
        pass

    @c_call("dds_notify_readers")
    def _notify_readers(self, subsriber: dds_c_t.entity) -> dds_c_t.returnv:
        pass


class DataReader(Entity):
    """Subscribe to a topic and read/take the data published to it.
    """

    def __init__(
            self,
            subscriber_or_participant: Union['cyclonedds.sub.Subscriber', 'cyclonedds.domain.DomainParticipant'],
            topic: 'cyclonedds.topic.Topic',
            qos: Optional['cyclonedds.core.Qos'] = None,
            listener: Optional['cyclonedds.core.Listener'] = None):
        """Initialize the DataReader

        Parameters
        ----------
        subscriber_or_participant: cyclonedds.sub.Subscriber, cyclonedds.domain.DomainParticipant
            The subscriber to which this reader will be added. If you supply a DomainParticipant a subscriber
            will be created for you.
        builtin_topic: cyclonedds.builtin.BuiltinTopic
            Which Builtin Topic to subscribe to. This can be one of BuiltinTopicDcpsParticipant, BuiltinTopicDcpsTopic,
            BuiltinTopicDcpsPublication or BuiltinTopicDcpsSubscription. Please note that BuiltinTopicDcpsTopic will fail if
            you built CycloneDDS without Topic Discovery.
        qos: cyclonedds.core.Qos, optional = None
            Optionally supply a Qos.
        listener: cyclonedds.core.Listener = None
            Optionally supply a Listener.
        """
        cqos = _CQos.qos_to_cqos(qos) if qos else None
        super().__init__(
            self._create_reader(
                subscriber_or_participant._ref,
                topic._ref,
                cqos,
                listener._ref if listener else None
            ),
            listener=listener
        )
        self._topic = topic
        self._next_condition = None
        if cqos:
            _CQos.cqos_destroy(cqos)

    def read(self, N: int = 1, condition: Entity = None, instance_handle: int = None) -> List[object]:
        """Read a maximum of N samples, non-blocking. Optionally use a read/query-condition to select which samples
        you are interested in.

        Parameters
        ----------
        N: int
            The maximum number of samples to read.
        condition: cyclonedds.core.ReadCondition, cyclonedds.core.QueryCondition, optional
            Only read samples that satisfy the supplied condition.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        if instance_handle is not None:
            ret = ddspy_read_handle(condition._ref if condition else self._ref, N, instance_handle)
        else:
            ret = ddspy_read(condition._ref if condition else self._ref, N)

        if type(ret) == int:
            raise DDSException(ret, f"Occurred while reading data in {repr(self)}")

        samples = []
        for (data, info) in ret:
            samples.append(self._topic.data_type.deserialize(data))
            samples[-1].sample_info = info
        return samples

    def take(self, N: int = 1, condition: Entity = None, instance_handle: int = None) -> List[object]:
        """Take a maximum of N samples, non-blocking. Optionally use a read/query-condition to select which samples
        you are interested in.

        Parameters
        ----------
        N: int
            The maximum number of samples to read.
        condition: cyclonedds.core.ReadCondition, cyclonedds.core.QueryCondition, optional
            Only take samples that satisfy the supplied condition.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        if instance_handle is not None:
            ret = ddspy_take_handle(condition._ref if condition else self._ref, N, instance_handle)
        else:
            ret = ddspy_take(condition._ref if condition else self._ref, N)

        if type(ret) == int:
            raise DDSException(ret, f"Occurred while taking data in {repr(self)}")

        samples = []
        for (data, info) in ret:
            samples.append(self._topic.data_type.deserialize(data))
            samples[-1].sample_info = info
        return samples

    def read_next(self) -> Optional[object]:
        """Shortcut method to read exactly one sample or return None.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        self._next_condition = self._next_condition or \
            ReadCondition(self, ViewState.Any | SampleState.NotRead | InstanceState.Any)
        samples = self.read(condition=self._next_condition)
        if samples:
            return samples[0]
        return None

    def take_next(self) -> Optional[object]:
        """Shortcut method to take exactly one sample or return None.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        self._next_condition = self._next_condition or \
            ReadCondition(self, ViewState.Any | SampleState.NotRead | InstanceState.Any)
        samples = self.take(condition=self._next_condition)
        if samples:
            return samples[0]
        return None

    def read_iter(self, condition=None, timeout: int = None) -> Generator[object, None, None]:
        """Shortcut method to iterate reading samples. Iteration will stop once the timeout you supply expires.
        Every time a sample is received the timeout is reset.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        waitset = WaitSet(self.participant)
        condition = ReadCondition(self, ViewState.Any | InstanceState.Any | SampleState.NotRead)
        waitset.attach(condition)
        timeout = timeout or duration(weeks=99999)

        while True:
            while True:
                a = self.read(condition=condition)
                if not a:
                    break
                yield a[0]
            if waitset.wait(timeout) == 0:
                break

    def take_iter(self, condition=None, timeout: int = None) -> Generator[object, None, None]:
        """Shortcut method to iterate taking samples. Iteration will stop once the timeout you supply expires.
        Every time a sample is received the timeout is reset.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        waitset = WaitSet(self.participant)
        condition = condition or ReadCondition(self, ViewState.Any | InstanceState.Any | SampleState.NotRead)
        waitset.attach(condition)
        timeout = timeout or duration(weeks=99999)

        while True:
            while True:
                a = self.take(condition=condition)
                if not a:
                    break
                yield a[0]
            if waitset.wait(timeout) == 0:
                break

    async def read_aiter(self, condition=None, timeout: int = None) -> AsyncGenerator[object, None]:
        """Shortcut method to asycn iterate reading samples. Iteration will stop once the timeout you supply expires.
        Every time a sample is received the timeout is reset.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        waitset = WaitSet(self.participant)
        condition = condition or ReadCondition(self, ViewState.Any | InstanceState.Any | SampleState.NotRead)
        waitset.attach(condition)
        timeout = timeout or duration(weeks=99999)

        loop = asyncio.get_running_loop()
        with concurrent.futures.ThreadPoolExecutor() as pool:
            while True:
                while True:
                    a = self.read(condition=condition)
                    if not a:
                        break
                    yield a[0]
                result = await loop.run_in_executor(pool, waitset.wait, timeout)
                if result == 0:
                    break

    async def take_aiter(self, condition=None, timeout: int = None) -> AsyncGenerator[object, None]:
        """Shortcut method to asycn iterate taking samples. Iteration will stop once the timeout you supply expires.
        Every time a sample is received the timeout is reset.

        Raises
        ------
        DDSException
            If any error code is returned by the DDS API it is converted into an exception.
        """
        waitset = WaitSet(self.participant)
        condition = condition or ReadCondition(self, ViewState.Any | InstanceState.Any | SampleState.NotRead)
        waitset.attach(condition)
        timeout = timeout or duration(weeks=99999)

        loop = asyncio.get_running_loop()
        with concurrent.futures.ThreadPoolExecutor() as pool:
            while True:
                while True:
                    a = self.take(condition=condition)
                    if not a:
                        break
                    yield a[0]
                result = await loop.run_in_executor(pool, waitset.wait, timeout)
                if result == 0:
                    break

    def wait_for_historical_data(self, timeout: int) -> bool:
        ret = self._wait_for_historical_data(self._ref, timeout)

        if ret == 0:
            return True
        elif ret == DDSException.DDS_RETCODE_TIMEOUT:
            return False
        raise DDSException(ret, f"Occured while waiting for historical data in {repr(self)}")

    def lookup_instance(self, sample):
        ret = ddspy_lookup_instance(self._ref, sample.serialize())
        if ret < 0:
            raise DDSException(ret, f"Occurred while lookup up instance from {repr(self)}")
        if ret == 0:
            return None
        return ret

    @c_call("dds_create_reader")
    def _create_reader(self, subscriber: dds_c_t.entity, topic: dds_c_t.entity, qos: dds_c_t.qos_p,
                       listener: dds_c_t.listener_p) -> dds_c_t.entity:
        pass

    @c_call("dds_reader_wait_for_historical_data")
    def _wait_for_historical_data(self, reader: dds_c_t.entity, max_wait: dds_c_t.duration) -> dds_c_t.returnv:
        pass


__all__ = ["Subscriber", "DataReader"]
