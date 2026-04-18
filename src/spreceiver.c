
#include "spreceiver.h"
#include "spconstants.h"
#include "sperror.h"
#include "spsignal.h"

#include <SoapySDR/Constants.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#include <complex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct spectrel_receiver_t
{
    SoapySDRDevice *device;
    SoapySDRStream *rx_stream;
    char *format;
};

int spectrel_free_receiver(spectrel_receiver receiver)
{
    if (!receiver)
    {
        return SPECTREL_SUCCESS;
    }

    if (receiver->format)
    {
        free(receiver->format);
        receiver->format = NULL;
    }
    if (receiver->device && receiver->rx_stream)
    {
        if (SoapySDRDevice_closeStream(receiver->device, receiver->rx_stream) !=
            0)
        {
            spectrel_print_error("closeStream failed: %s",
                                 SoapySDRDevice_lastError());
            return SPECTREL_FAILURE;
        }
        receiver->rx_stream = NULL;
    }

    if (receiver->device)
    {
        if (SoapySDRDevice_unmake(receiver->device) != 0)
        {
            spectrel_print_error("unmake failed: %s",
                                 SoapySDRDevice_lastError());
            return SPECTREL_FAILURE;
        }
        receiver->device = NULL;
    }
    free(receiver);
    return SPECTREL_SUCCESS;
}

static bool is_value_in_ranges(double value,
                               const SoapySDRRange *ranges,
                               size_t range_count)
{
    for (size_t i = 0; i < range_count; i++)
    {
        SoapySDRRange range = ranges[i];
        if (value >= range.minimum && value <= range.maximum)
        {
            return true;
        }
    }
    return false;
}

static bool is_value_in_range(double value, const SoapySDRRange *range)
{
    return value >= range->minimum && value <= range->maximum;
}

static const char *get_format(const char *driver)
{
    if (strcmp(driver, "rtlsdr") == 0)
    {
        return "CF32";
    }
    else if (strcmp(driver, "hackrf") == 0)
    {
        return "CF64";
    }
    else if (strcmp(driver, "SDDC") == 0)
    {
        return "CF32";
    }
    else
    {
        return SPECTREL_DEFAULT_FORMAT;
    }
}

static bool match(const char *string, char **strings, size_t num_strings)
{
    for (size_t n = 0; n < num_strings; n++)
    {
        if (strcmp(string, strings[n]) == 0)
        {
            return true;
        }
    }
    return false;
}

spectrel_receiver spectrel_make_receiver(const char *driver,
                                         spectrel_receiver_params_t *params)
{

    int status = SPECTREL_FAILURE;

    spectrel_receiver receiver = NULL;
    receiver = malloc(sizeof(*receiver));
    if (!receiver)
    {
        spectrel_print_error("malloc failed: receiver");
        return NULL;
    }

    receiver->device = NULL;
    receiver->rx_stream = NULL;
    receiver->format = NULL;

    size_t range_size = 0;
    SoapySDRRange *frequency_ranges = NULL;
    SoapySDRRange *sample_rate_ranges = NULL;
    SoapySDRRange *bandwidth_ranges = NULL;

    size_t num_formats = 0;
    char **formats = NULL;

    // Make the Soapy device.
    SoapySDRKwargs args = {};
    if (SoapySDRKwargs_set(&args, "driver", driver) != 0)
    {
        spectrel_print_error("set failed: Allocation error");
        goto cleanup;
    }
    receiver->device = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (!receiver->device)
    {
        spectrel_print_error("make failed: %s", SoapySDRDevice_lastError());
        goto cleanup;
    }

    // Set device parameters. For each, first validate it against device info if
    // it's available. If there's no device info, just try and set it anyway.
    frequency_ranges = SoapySDRDevice_getFrequencyRange(
        receiver->device, SOAPY_SDR_RX, 0, &range_size);
    if (!frequency_ranges)
    {
        spectrel_print_error("getFrequencyRange failed: %s",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }
    if (range_size > 0 &&
        !is_value_in_ranges(params->frequency, frequency_ranges, range_size))
    {
        spectrel_print_error("Invalid frequency: %lf [Hz]", params->frequency);
        goto cleanup;
    }
    if (SoapySDRDevice_setFrequency(
            receiver->device, SOAPY_SDR_RX, 0, params->frequency, NULL) != 0)
    {
        spectrel_print_error("setFrequency fail: %s",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }

    sample_rate_ranges = SoapySDRDevice_getSampleRateRange(
        receiver->device, SOAPY_SDR_RX, 0, &range_size);
    if (!sample_rate_ranges)
    {
        spectrel_print_error("getSampleRateRanges fail: %s\n",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }
    if (range_size > 0 && !is_value_in_ranges(params->sample_rate,
                                              sample_rate_ranges,
                                              range_size))
    {
        spectrel_print_error("Invalid sample rate: %lf [Hz]",
                             params->sample_rate);
        goto cleanup;
    }
    if (SoapySDRDevice_setSampleRate(
            receiver->device, SOAPY_SDR_RX, 0, params->sample_rate) != 0)
    {
        spectrel_print_error("setSampleRate failed: %s\n",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }

    // Set the bandwidth.
    bandwidth_ranges = SoapySDRDevice_getBandwidthRange(
        receiver->device, SOAPY_SDR_RX, 0, &range_size);
    if (!bandwidth_ranges)
    {
        spectrel_print_error("getBandwidthRange failed: %s\n",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }
    if (range_size > 0 &&
        !is_value_in_ranges(params->bandwidth, bandwidth_ranges, range_size))
    {
        spectrel_print_error("Invalid bandwidth: %lf", params->bandwidth);
        goto cleanup;
    }
    if (SoapySDRDevice_setBandwidth(
            receiver->device, SOAPY_SDR_RX, 0, params->bandwidth) != 0)
    {
        spectrel_print_error("setBandwidth failed: %s",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }

    SoapySDRRange gain_range =
        SoapySDRDevice_getGainRange(receiver->device, SOAPY_SDR_RX, 0);
    if (!is_value_in_range(params->gain, &gain_range))
    {
        spectrel_print_error("Invalid gain: %lf", params->gain);
        goto cleanup;
    }

    // TODO: Make the stream format configurable.
    const char *format = get_format(driver);
    formats = SoapySDRDevice_getStreamFormats(
        receiver->device, SOAPY_SDR_RX, 0, &num_formats);
    if (!formats)
    {
        spectrel_print_error("getStreamFormats failed: %s\n",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }
    if (!match(format, formats, num_formats))
    {
        spectrel_print_error("Invalid format: %s\n", format);
        goto cleanup;
    }

    receiver->format = strdup(format);
    if (!receiver->format)
    {
        spectrel_print_error("strdup failed: format");
        goto cleanup;
    }

    // Set up the stream.
    receiver->rx_stream = SoapySDRDevice_setupStream(
        receiver->device, SOAPY_SDR_RX, receiver->format, NULL, 0, NULL);
    if (!receiver->rx_stream)
    {
        spectrel_print_error("setupStream failed: %s",
                             SoapySDRDevice_lastError());
        goto cleanup;
    }

    status = SPECTREL_SUCCESS;

cleanup:
    if (formats)
    {
        SoapySDRStrings_clear(&formats, num_formats);
    }
    if (bandwidth_ranges)
    {
        SoapySDR_free(bandwidth_ranges);
    }
    if (sample_rate_ranges)
    {
        SoapySDR_free(sample_rate_ranges);
    }
    if (frequency_ranges)
    {
        SoapySDR_free(frequency_ranges);
    }
    if (status != SPECTREL_SUCCESS && receiver)
    {
        spectrel_free_receiver(receiver);
        receiver = NULL;
    }
    return receiver;
}

int spectrel_get_parameters(spectrel_receiver receiver,
                            spectrel_receiver_params_t *params)
{
    if (!params)
    {
        return SPECTREL_FAILURE;
    }
    params->frequency =
        SoapySDRDevice_getFrequency(receiver->device, SOAPY_SDR_RX, 0);
    params->sample_rate =
        SoapySDRDevice_getSampleRate(receiver->device, SOAPY_SDR_RX, 0);
    params->bandwidth =
        SoapySDRDevice_getBandwidth(receiver->device, SOAPY_SDR_RX, 0);
    params->gain = SoapySDRDevice_getGain(receiver->device, SOAPY_SDR_RX, 0);
    return SPECTREL_SUCCESS;
}

int spectrel_activate_stream(spectrel_receiver receiver)
{
    if (SoapySDRDevice_activateStream(
            receiver->device, receiver->rx_stream, 0, 0, 0) != 0)
    {
        spectrel_print_error("activateStream failed: %s",
                             SoapySDRDevice_lastError());
        return SPECTREL_FAILURE;
    }
    return SPECTREL_SUCCESS;
}

int spectrel_deactivate_stream(spectrel_receiver receiver)
{
    if (SoapySDRDevice_deactivateStream(
            receiver->device, receiver->rx_stream, 0, 0) != 0)
    {
        spectrel_print_error("deactivateStream failed: %s",
                             SoapySDRDevice_lastError());
        return SPECTREL_FAILURE;
    }
    return SPECTREL_SUCCESS;
}

int spectrel_read_stream(spectrel_receiver receiver, spectrel_signal_t *buffer)
{
    int num_samples_read = 0;
    void *buffers[] = {NULL};
    int flags;
    long long timeNs;
    int ret;

    // The buffer passed in by the caller is compatable with the format
    // SOAPY_SDR_CF64, but not SOAPY_SDR_CF32
    bool needs_conversion = strcmp(receiver->format, SOAPY_SDR_CF32) == 0;

    if (!needs_conversion)
    {
        // If the buffer format is compatable with the device format, fill it
        // directly.
        while (num_samples_read < buffer->num_samples)
        {
            buffers[0] = (void *)(buffer->samples + num_samples_read);
            ret = SoapySDRDevice_readStream(receiver->device,
                                            receiver->rx_stream,
                                            buffers,
                                            buffer->num_samples -
                                                num_samples_read,
                                            &flags,
                                            &timeNs,
                                            SPECTREL_TIMEOUT);

            if (ret < 1)
            {
                spectrel_print_error("readStream failed: %s\n",
                                     SoapySDRDevice_lastError());
                return SPECTREL_FAILURE;
            }
            num_samples_read += ret;
        }
        return SPECTREL_SUCCESS;
    }
    else
    {
        // Otherwise, fill an intermediate buffer with a compatible format.
        complex float *buffer_cf32 =
            malloc(buffer->num_samples * sizeof(complex float));
        if (!buffer_cf32)
        {
            spectrel_print_error("malloc failed: buffer_cf32");
            return SPECTREL_FAILURE;
        }

        while (num_samples_read < buffer->num_samples)
        {
            buffers[0] = buffer_cf32 + num_samples_read;
            int ret = SoapySDRDevice_readStream(receiver->device,
                                                receiver->rx_stream,
                                                buffers,
                                                buffer->num_samples -
                                                    num_samples_read,
                                                &flags,
                                                &timeNs,
                                                SPECTREL_TIMEOUT);

            if (ret < 1)
            {
                const char *error = SoapySDRDevice_lastError();
                spectrel_print_error("readStream failed: %s\n", error);
                free(buffer_cf32);
                buffer_cf32 = NULL;
                return SPECTREL_FAILURE;
            }
            num_samples_read += ret;
        }

        // Finally, type cast and copy the samples into the buffer passed in by
        // the caller.
        for (size_t n = 0; n < buffer->num_samples; n++)
        {
            buffer->samples[n] = (complex double)buffer_cf32[n];
        }
        free(buffer_cf32);
        buffer_cf32 = NULL;
        return SPECTREL_SUCCESS;
    }
}

void spectrel_describe_receiver(spectrel_receiver receiver)
{
    spectrel_receiver_params_t params = {};
    spectrel_get_parameters(receiver, &params);
    printf("Parameters set:\n");
    printf("  Frequency: %.4lf [Hz]\n", params.frequency);
    printf("  Sample rate: %.4lf [Hz]\n", params.sample_rate);
    printf("  Bandwidth: %.4lf [Hz]\n", params.bandwidth);
    printf("  Gain: %.4lf [dB]\n", params.gain);
    printf("  Format: %s\n", receiver->format);
}