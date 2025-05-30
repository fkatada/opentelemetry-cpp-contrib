// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/user_events/logs/exporter.h"
#include "opentelemetry/exporters/user_events/logs/recordable.h"
#include "opentelemetry/sdk_config.h"

namespace nostd     = opentelemetry::nostd;
namespace sdklogs   = opentelemetry::sdk::logs;
namespace sdkcommon = opentelemetry::sdk::common;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace user_events
{
namespace logs
{

/*********************** Constructor ***********************/

Exporter::Exporter(const ExporterOptions &options) noexcept
    : options_(options), provider_(options.provider_name)
{
  // Initialize the event sets
  for (int i = 0; i < sizeof(event_levels_map) / sizeof(event_levels_map[0]); i++)
  {
    event_set_levels_[i] = provider_.RegisterSet(event_levels_map[i], 1);
  }
}

/*********************** Exporter methods ***********************/

std::unique_ptr<sdk_logs::Recordable> Exporter::MakeRecordable() noexcept
{
  return std::unique_ptr<Recordable>(new Recordable());
}

sdk::common::ExportResult Exporter::Export(
    const nostd::span<std::unique_ptr<sdklogs::Recordable>> &records) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[user_events Log Exporter] Exporting "
                            << records.size() << " log(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }

  int err;

  for (auto &record : records)
  {
    auto user_events_record =
        std::unique_ptr<Recordable>(static_cast<Recordable *>(record.release()));

    if (!user_events_record->PrepareExport())
    {
      continue;
    }

    int level_index = user_events_record->GetLevelIndex();

    auto event_set = event_set_levels_[level_index];

    if (event_set->Enabled())
    {
      err = user_events_record->GetEventBuilder().Write(*event_set_levels_[level_index]);
    }
    else
    {
      // event_set is not enabled
      err = 0;
    }

    if (err != 0)
    {
      OTEL_INTERNAL_LOG_ERROR("[user_events Log Exporter] Exporting failed, error code: " << err);
      return sdk::common::ExportResult::kFailure;
    }
  }

  return sdk::common::ExportResult::kSuccess;
}

bool Exporter::Shutdown(std::chrono::microseconds) noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  is_shutdown_ = true;
  return true;
}

bool Exporter::isShutdown() const noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  return is_shutdown_;
}

}  // namespace logs
}  // namespace user_events
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
