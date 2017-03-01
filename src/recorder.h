// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#pragma once

#include "backend.h"
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>
#include <map>

namespace rsimpl
{
    namespace uvc
    {
        enum class call_type
        {
            none,
            query_uvc_devices,
            query_usb_devices,
            send_command,
            create_usb_device,
            create_uvc_device,
            uvc_get_location,
            uvc_set_power_state,
            uvc_get_power_state,
            uvc_lock,
            uvc_unlock,
            uvc_get_pu,
            uvc_set_pu,
            uvc_get_pu_range,
            uvc_get_xu_range,
            uvc_init_xu,
            uvc_set_xu,
            uvc_get_xu,
            uvc_stream_profiles,
            uvc_probe_commit,
            uvc_play,
            uvc_start_callbacks,
            uvc_stop_callbacks,
            uvc_close,
            uvc_frame,
            create_hid_device,
            query_hid_devices,
            hid_open,
            hid_close,
            hid_stop_capture,
            hid_start_capture,
            hid_frame,
            hid_get_sensors
        };

        class compression_algorithm
        {
        public:
            int dist(uint32_t x, uint32_t y) const;

            std::vector<uint8_t> decode(const std::vector<uint8_t>& input) const;

            std::vector<uint8_t> encode(uint8_t* data, uint32_t size) const;

            int min_dist = 110;
            int max_length = 32;
        };

        struct call
        {
            call_type type = call_type::none;
            double timestamp = 0;
            int entity_id = 0;
            std::string inline_string;

            int param1 = 0;
            int param2 = 0;
            int param3 = 0;
            int param4 = 0;
            int param5 = 0;
            int param6 = 0;

            bool had_error = false;
        };

        struct lookup_key
        {
            int entity_id;
            call_type type;
        };

        class recording
        {
        public:
            recording(std::shared_ptr<time_service> ts = nullptr);

            double get_time();
            void save(const char* filename, const char* section, bool append = false) const;
            static std::shared_ptr<recording> load(const char* filename, const char* section);

            int save_blob(const void* ptr, unsigned int size);



            template<class T>
            void save_list(std::vector<T> list, std::vector<T>& target, call_type type, int entity_id)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                call c;
                c.type = type;
                c.entity_id = entity_id;
                c.param1 = static_cast<int>(target.size());
                for (auto&& i : list) target.push_back(i);
                c.param2 = static_cast<int>(target.size());

                c.timestamp = get_current_time();
                calls.push_back(c);
            }

            call& add_call(lookup_key key)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                call c;
                c.type = key.type;
                c.entity_id = key.entity_id;
                c.timestamp = get_current_time();
                calls.push_back(c);
                return calls[calls.size() - 1];
            }

            template<class T>
            std::vector<T> load_list(const std::vector<T>& source, const call& c)
            {
                std::vector<T> results;
                std::lock_guard<std::mutex> lock(_mutex);
                for (auto i = c.param1; i < c.param2; i++)
                {
                    results.push_back(source[i]);
                }
                return results;
            }

            void save_device_info_list(std::vector<uvc_device_info> list, lookup_key k)
            {
                save_list(list, uvc_device_infos, k.type, k.entity_id);
            }

            void save_device_info_list(std::vector<hid_device_info> list, lookup_key k)
            {
                save_list(list, hid_device_infos, k.type, k.entity_id);
            }

            void save_device_info_list(std::vector<usb_device_info> list, lookup_key k)
            {
                save_list(list, usb_device_infos, k.type, k.entity_id);
            }

            void save_stream_profiles(std::vector<stream_profile> list, lookup_key key)
            {
                save_list(list, stream_profiles, key.type, key.entity_id);
            }

            void save_hid_sensors(std::vector<hid_sensor> list, lookup_key key)
            {
                save_list(list, hid_sensors, key.type, key.entity_id);
            }

            void save_hid_sensors_inputs(std::vector<hid_sensor_input> list, lookup_key key)
            {
                save_list(list, hid_sensor_inputs, key.type, key.entity_id);
            }

            std::vector<stream_profile> load_stream_profiles(int id, call_type type)
            {
                auto&& c = find_call(type, id);
                return load_list(stream_profiles, c);
            }

            std::vector<usb_device_info> load_usb_device_info_list()
            {
                auto&& c = find_call(call_type::query_usb_devices, 0);
                return load_list(usb_device_infos, c);
            }

            std::vector<uvc_device_info> load_uvc_device_info_list()
            {
                auto&& c = find_call(call_type::query_uvc_devices, 0);
                return load_list(uvc_device_infos, c);
            }

            std::vector<hid_device_info> load_hid_device_info_list()
            {
                auto&& c = find_call(call_type::query_hid_devices, 0);
                return load_list(hid_device_infos, c);
            }

            std::vector<hid_sensor> load_hid_sensors_list(int entity_id)
            {
                auto&& c = find_call(call_type::hid_get_sensors, entity_id);
                return load_list(hid_sensors, c);
            }

            std::vector<uint8_t> load_blob(int id) const
            {
                return blobs[id];
            }

            call& find_call(call_type t, int entity_id, std::function<bool(const call& c)> history_match_validation = [](const call& c) {return true; });
            call* cycle_calls(call_type call_type, int id);

            size_t size() const { return calls.size(); }

        private:
            std::vector<call> calls;
            std::vector<std::vector<uint8_t>> blobs;
            std::vector<uvc_device_info> uvc_device_infos;
            std::vector<usb_device_info> usb_device_infos;
            std::vector<stream_profile> stream_profiles;
            std::vector<hid_device_info> hid_device_infos;
            std::vector<hid_sensor> hid_sensors;
            std::vector<hid_sensor_input> hid_sensor_inputs;

            std::mutex _mutex;
            std::shared_ptr<time_service> _ts;

            std::map<int, int> _cursors;
            std::map<int, int> _cycles;

            double get_current_time();

            double _curr_time = 0;
        };

        class record_backend;

        class record_uvc_device : public uvc_device
        {
        public:
            void probe_and_commit(stream_profile profile, frame_callback callback) override;
            void stream_on() override;
            void start_callbacks() override;
            void stop_callbacks() override;
            void close(stream_profile profile) override;
            void set_power_state(power_state state) override;
            power_state get_power_state() const override;
            void init_xu(const extension_unit& xu) override;
            void set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len) override;
            void get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t ctrl, int len) const override;
            int get_pu(rs_option opt) const override;
            void set_pu(rs_option opt, int value) override;
            control_range get_pu_range(rs_option opt) const override;
            std::vector<stream_profile> get_profiles() const override;
            void lock() const override;
            void unlock() const override;
            std::string get_device_location() const override;

            explicit record_uvc_device(
                std::shared_ptr<uvc_device> source,
                std::shared_ptr<compression_algorithm> compression,
                int id, const record_backend* owner)
                : _source(source), _entity_id(id),
                _compression(compression), _owner(owner) {}

        private:
            std::shared_ptr<uvc_device> _source;
            int _entity_id;
            std::shared_ptr<compression_algorithm> _compression;
            const record_backend* _owner;
        };

        class record_hid_device : public hid_device
        {
        public:
            void open() override;
            void close() override;
            void stop_capture() override;
            void start_capture(const std::vector<iio_profile>& iio_profiles, hid_callback callback) override;
            std::vector<hid_sensor> get_sensors() override;

            record_hid_device(std::shared_ptr<hid_device> source,
                int id, const record_backend* owner)
                : _source(source), _entity_id(id), _owner(owner) {}

        private:
            std::shared_ptr<hid_device> _source;
            int _entity_id;
            const record_backend* _owner;
        };

        class record_usb_device : public usb_device
        {
        public:
            std::vector<uint8_t> send_receive(const std::vector<uint8_t>& data, int timeout_ms, bool require_response) override;

            record_usb_device(std::shared_ptr<usb_device> source,
                int id, const record_backend* owner)
                : _source(source), _entity_id(id), _owner(owner) {}

        private:
            std::shared_ptr<usb_device> _source;
            int _entity_id;
            const record_backend* _owner;
        };

        class record_backend : public backend
        {
        public:
            std::shared_ptr<hid_device> create_hid_device(hid_device_info info) const override;
            std::vector<hid_device_info> query_hid_devices() const override;
            std::shared_ptr<uvc_device> create_uvc_device(uvc_device_info info) const override;
            std::vector<uvc_device_info> query_uvc_devices() const override;
            std::shared_ptr<usb_device> create_usb_device(usb_device_info info) const override;
            std::vector<usb_device_info> query_usb_devices() const override;
            std::shared_ptr<time_service> create_time_service() const override;

            record_backend(std::shared_ptr<backend> source,
                const char* filename,
                const char* section,
                rs_recording_mode mode);
            ~record_backend();

            rs_recording_mode get_mode() const { return _mode; }

            template<class T>
            auto try_record(T t, int entity_id, call_type type) const
                -> decltype(t((recording*)nullptr, *((lookup_key*)nullptr)))
            {
                lookup_key k{ entity_id, type };

                try
                {
                    return t(_rec.get(), k);
                }
                catch (const std::exception& ex)
                {
                    auto&& c = _rec->add_call(k);
                    c.had_error = true;
                    c.inline_string = ex.what();

                    throw;
                }
                catch (...)
                {
                    auto&& c = _rec->add_call(k);
                    c.had_error = true;
                    c.inline_string = "Unknown exception has occured!";

                    throw;
                }
            }

        private:
            void write_to_file() const;

            std::shared_ptr<backend> _source;

            std::shared_ptr<recording> _rec;
            mutable std::atomic<int> _entity_count;
            std::string _filename;
            std::string _section;
            std::shared_ptr<compression_algorithm> _compression;
            rs_recording_mode _mode;
        };

        typedef std::vector<std::pair<stream_profile, frame_callback>> configurations;

        class playback_uvc_device : public uvc_device
        {
        public:
            void probe_and_commit(stream_profile profile, frame_callback callback) override;
            void stream_on() override;
            void start_callbacks() override;
            void stop_callbacks() override;
            void close(stream_profile profile) override;
            void set_power_state(power_state state) override;
            power_state get_power_state() const override;
            void init_xu(const extension_unit& xu) override;
            void set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len) override;
            void get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t ctrl, int len) const override;
            int get_pu(rs_option opt) const override;
            void set_pu(rs_option opt, int value) override;
            control_range get_pu_range(rs_option opt) const override;
            std::vector<stream_profile> get_profiles() const override;
            void lock() const override;
            void unlock() const override;
            std::string get_device_location() const override;

            explicit playback_uvc_device(std::shared_ptr<recording> rec,int id);

            void callback_thread();
            ~playback_uvc_device();

        private:
            std::shared_ptr<recording> _rec;
            int _entity_id;
            std::atomic<bool> _alive;
            std::thread _callback_thread;
            configurations _callbacks;
            configurations _commitments;
            std::mutex _callback_mutex;
            compression_algorithm _compression;
        };


        class playback_usb_device : public usb_device
        {
        public:
            std::vector<uint8_t> send_receive(const std::vector<uint8_t>& data, int timeout_ms, bool require_response) override;

            explicit playback_usb_device(std::shared_ptr<recording> rec,
                int id) : _rec(rec), _entity_id(id) {}

        private:
            std::shared_ptr<recording> _rec;
            int _entity_id;
        };

        class playback_hid_device : public hid_device
        {
        public:
            void open() override;
            void close() override;
            void stop_capture() override;
            void start_capture(const std::vector<iio_profile>& iio_profiles, hid_callback callback) override;
            std::vector<hid_sensor> get_sensors() override;

            void callback_thread();
            ~playback_hid_device();

            explicit playback_hid_device(std::shared_ptr<recording> rec, int id);

        private:
            std::shared_ptr<recording> _rec;
            std::mutex _callback_mutex;
            uvc::hid_callback _callback;
            int _entity_id;
            std::thread _callback_thread;
            std::atomic<bool> _alive;
        };

        class playback_backend : public backend
        {
        public:
            std::shared_ptr<hid_device> create_hid_device(hid_device_info info) const override;
            std::vector<hid_device_info> query_hid_devices() const override;
            std::shared_ptr<uvc_device> create_uvc_device(uvc_device_info info) const override;
            std::vector<uvc_device_info> query_uvc_devices() const override;
            std::shared_ptr<usb_device> create_usb_device(usb_device_info info) const override;
            std::vector<usb_device_info> query_usb_devices() const override;
            std::shared_ptr<time_service> create_time_service() const override;

            explicit playback_backend(const char* filename, const char* section);
        private:
            std::shared_ptr<recording> _rec;
        };

        class recording_time_service: public time_service
        {
        public:
            recording_time_service(recording& rec):
                _rec(rec){}

            virtual double get_time() const override
            {
              return _rec.get_time();
            }
        private:
            recording& _rec;
        };
    }
}