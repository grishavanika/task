#include <task_curl/task_curl.h>

nn::curl::detail::CurlTask::CurlTask(CurlGet&& curl_get)
	: buffer_()
	, data_()
	, multi_handle_(nullptr)
	, request_(nullptr)
{
	if (!setup(curl_get))
	{
		set_generic_error();
		cleanup();
	}
}

bool nn::curl::detail::CurlTask::setup(const CurlGet& options)
{
	multi_handle_ = curl_multi_init();
	if (!multi_handle_)
	{
		return false;
	}
	request_ = curl_easy_init();
	if (!request_)
	{
		return false;
	}

	bool ok = true;
	const long verbose = options.verbose ? 1L : 0L;
	ok &= (curl_easy_setopt(request_, CURLOPT_URL, options.url.c_str()) == CURLE_OK);
	ok &= (curl_easy_setopt(request_, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK);
	ok &= (curl_easy_setopt(request_, CURLOPT_VERBOSE, verbose) == CURLE_OK);
	ok &= (curl_easy_setopt(request_, CURLOPT_WRITEDATA, this) == CURLE_OK);
	ok &= (curl_easy_setopt(request_, CURLOPT_WRITEFUNCTION, &CurlTask::WriteCallback) == CURLE_OK);

	ok &= (curl_multi_add_handle(multi_handle_, request_) == CURLM_OK);

	return ok;
}

void nn::curl::detail::CurlTask::cleanup()
{
	if (multi_handle_ && request_)
	{
		(void)curl_multi_remove_handle(multi_handle_, request_);
	}
	if (request_)
	{
		curl_easy_cleanup(request_);
	}
	if (multi_handle_)
	{
		(void)curl_multi_cleanup(multi_handle_);
	}
}

nn::curl::detail::CurlTask::~CurlTask()
{
	cleanup();
}

nn::Status nn::curl::detail::CurlTask::tick(const ExecutionContext& context)
{
	if (!multi_handle_)
	{
		set_generic_error();
		return Status::Failed;
	}

	if (context.cancel_requested)
	{
		set_generic_error();
		cleanup();
		return Status::Canceled;
	}

	int running = 0;
	CURLMcode status = CURLM_CALL_MULTI_PERFORM;
	while (status == CURLM_CALL_MULTI_PERFORM)
	{
		status = curl_multi_perform(multi_handle_, &running);
	}
	if ((running > 0) && (status == CURLM_OK))
	{
		return Status::InProgress;
	}
	else if ((running == 0) && (status == CURLM_OK))
	{
		return on_finish();
	}
	assert(running == 0);
	return on_poll_error(status);
}

nn::Status nn::curl::detail::CurlTask::on_finish()
{
	data_ = std::move(buffer_);
	return Status::Successful;
}

nn::Status nn::curl::detail::CurlTask::on_poll_error(CURLMcode code)
{
	(void)code;
	set_generic_error();
	return Status::Failed;
}

void nn::curl::detail::CurlTask::set_generic_error()
{
	SetExpectedWithError(data_, CurlError(true));
}

nn::expected<nn::curl::Buffer, nn::curl::CurlError>& nn::curl::detail::CurlTask::get()
{
	return data_;
}

/*static*/ size_t nn::curl::detail::CurlTask::WriteCallback(
	char* ptr, size_t size, size_t nmemb, void* userdata)
{
	(void)size;
	CurlTask& self = *static_cast<CurlTask*>(userdata);
	self.buffer_.insert(std::end(self.buffer_), ptr, ptr + nmemb);
	return nmemb;
}
