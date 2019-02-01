#pragma once
#include <rename_me/task.h>

#include <string>
#include <vector>

#include <cassert>

#include <curl/curl.h>

namespace nn
{
	namespace curl
	{

		using Buffer = std::vector<char>;

		struct CurlGet
		{
			std::string url;
			bool verbose = false;

			CurlGet& set_url(std::string str)
			{
				url = std::move(str);
				return *this;
			}

			CurlGet& set_verbose(bool enable)
			{
				verbose = enable;
				return *this;
			}
		};

		struct CurlError
		{
			bool init_error;
			explicit CurlError(bool e)
				: init_error(e)
			{
			}
		};

		namespace detail
		{

			class CurlTask
			{
				using Error = unexpected<CurlError>;
			public:
				explicit CurlTask(CurlGet&& curl_get);
				~CurlTask();

				bool setup(const CurlGet& options);
				void cleanup();

				Status tick(bool cancel_requested);
				expected<Buffer, CurlError>& get();

				Status on_finish();
				Status on_poll_error(CURLMcode code);

				static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

			private:
				Buffer buffer_;
				expected<Buffer, CurlError> data_;
				CURLM* multi_handle_;
				CURL* request_;
			};

		} // namespace detail

		inline Task<Buffer, CurlError> make_task(
			Scheduler& scheduler, CurlGet curl_get)
		{
			using Task = Task<Buffer, CurlError>;
			return Task::template make<detail::CurlTask>(scheduler, std::move(curl_get));
		}

	} // namespace curl

} // namespace nn
