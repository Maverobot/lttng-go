#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <babeltrace2/babeltrace.h>
#include <json-c/json.h>

#include "fail_fast_if.h"

static void AddFieldStruct(json_object* jobj, char const* structName, const bt_field* field);
static void AddField(json_object* jobj, char const* fieldName, const bt_field* field);

static bool startsWith(const char* str, const char* query_prefix) {
  if (!str || !query_prefix) {
    return false;
  }

  const size_t lenstr = strlen(str);
  const size_t lenpre = strlen(query_prefix);
  return lenpre > lenstr ? false : strncmp(str, query_prefix, lenpre) == 0;
}

static bool endsWith(const char* str, const char* query_suffix) {
  if (!str || !query_suffix) {
    return false;
  }

  const size_t lenstr = strlen(str);
  const size_t lensuf = strlen(query_suffix);
  return lensuf > lenstr ? false : strncmp(str + lenstr - lensuf, query_suffix, lensuf) == 0;
}

void AddFieldInteger(json_object* jobj, char const* const fieldName, const bt_field* field) {
  uint64_t val = bt_field_integer_unsigned_get_value(field);

  if (fieldName) {
    json_object_object_add(jobj, fieldName, json_object_new_int64(val));
  } else {
    FAIL_FAST_IF(!json_object_is_type(jobj, json_type_array));
    json_object_array_add(jobj, json_object_new_int64(val));
  }
}

static void AddFieldBool(json_object* jobj, char const* const fieldName, const bt_field* field) {
  bool val = bt_field_bool_get_value(field) == BT_TRUE;
  json_object_object_add(jobj, fieldName, json_object_new_boolean(val));
}

static void AddFieldString(json_object* jobj, char const* fieldName, const bt_field* field) {
  const char* val = bt_field_string_get_value(field);
  uint64_t len = bt_field_string_get_length(field);
  json_object_object_add(jobj, fieldName, json_object_new_string(val));
}

static void AddFieldReal(json_object* jobj, char const* fieldName, const bt_field* field) {
  float val = bt_field_real_single_precision_get_value(field);
  json_object_object_add(jobj, fieldName, json_object_new_double(val));
}

static void AddFieldBitArray(json_object* jobj,
                             char const* const fieldName,
                             const bt_field* field) {
  uint64_t val = bt_field_bit_array_get_value_as_integer(field);
  json_object_object_add(jobj, fieldName, json_object_new_int64(val));
}

static void AddFieldArray(json_object* jobj, char const* fieldName, const bt_field* field) {
  json_object* array_jobj = json_object_new_array();
  json_object_object_add(jobj, fieldName, array_jobj);

  uint64_t numElements = bt_field_array_get_length(field);
  for (uint64_t i = 0; i < numElements; i++) {
    const bt_field* elementField = bt_field_array_borrow_element_field_by_index_const(field, i);

    AddField(array_jobj, NULL, elementField);
  }
}

static void AddFieldEnum(json_object* jobj, char const* const fieldName, const bt_field* field) {
  const char* const* labels = NULL;
  uint64_t labelsCount = 0;
  bt_field_enumeration_unsigned_get_mapping_labels(field, &labels, &labelsCount);

  if (labelsCount > 0) {
    json_object_object_add(jobj, fieldName, json_object_new_string(labels[0]));
  } else {
    uint64_t val = bt_field_integer_unsigned_get_value(field);
    json_object_object_add(jobj, fieldName, json_object_new_int64(val));
  }
}

static void AddField(json_object* jobj, char const* fieldName, const bt_field* field) {
  // Skip added '_foo_sequence_field_length' type fields
  if (startsWith(fieldName, "_") && endsWith(fieldName, "_length")) {
    return;
  }

  bt_field_class_type fieldType = bt_field_get_class_type(field);
  switch (fieldType) {
    case BT_FIELD_CLASS_TYPE_BOOL:
      AddFieldBool(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
      AddFieldBitArray(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
    case BT_FIELD_CLASS_TYPE_INTEGER:
    case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
      AddFieldInteger(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_ENUMERATION:
    case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
    case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
      AddFieldEnum(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_REAL:
    case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
    case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
      AddFieldReal(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_STRING:
      AddFieldString(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_STRUCTURE:
      AddFieldStruct(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
    case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
    case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
    case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
      AddFieldArray(jobj, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
      // AddFieldOption(builder, itr, fieldName, field);
      return;
    case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
      // AddFieldVariant(builder, itr, fieldName, field);
      return;
      // Unimplemented
    case BT_FIELD_CLASS_TYPE_ARRAY:
    case BT_FIELD_CLASS_TYPE_OPTION:
    case BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_VARIANT:
    case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD:
    case BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD:
    case __BT_FIELD_CLASS_TYPE_BIG_VALUE:
      printf("WARNING: Field decoder is not implemented\n");
      return;
  }
  printf("WARNING: not all field types have been listed.\n");
}

static void AddFieldStruct(json_object* jobj, char const* structName, const bt_field* field) {
  const bt_field_class* fieldClass = bt_field_borrow_class_const(field);
  uint64_t numFields = bt_field_class_structure_get_member_count(fieldClass);
  json_object* struct_jobj = json_object_new_object();

  for (uint64_t i = 0; i < numFields; i++) {
    const bt_field_class_structure_member* structFieldClass =
        bt_field_class_structure_borrow_member_by_index_const(fieldClass, i);

    const char* structFieldName = bt_field_class_structure_member_get_name(structFieldClass);
    const bt_field* structField = bt_field_structure_borrow_member_field_by_index_const(field, i);

    AddField(struct_jobj, structFieldName, structField);
  }

  json_object_object_add(jobj, structName, struct_jobj);
}

static void AddEventName(json_object* const jobj, bt_event_class const* const event_class) {
  json_object_object_add(jobj, "name",
                         json_object_new_string(bt_event_class_get_name(event_class)));
}

static void AddPayload(json_object* const jobj, bt_event const* const event) {
  const bt_field* payloadField = bt_event_borrow_payload_field_const(event);
  AddFieldStruct(jobj, "payload", payloadField);
}

static void AddTimestamp(json_object* jobj, const bt_clock_snapshot* clock) {
  int64_t nanosFromEpoch = 0;
  bt_clock_snapshot_get_ns_from_origin_status clockStatus =
      bt_clock_snapshot_get_ns_from_origin(clock, &nanosFromEpoch);
  FAIL_FAST_IF(clockStatus != BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK);
  time_t t = (time_t)(nanosFromEpoch / 1e9);
  json_object_object_add(jobj, "time", json_object_new_string(strtok(ctime(&t), "\n")));
}

static void AddPacketContext(json_object* jobj, const bt_event* event) {
  const bt_packet* packet = bt_event_borrow_packet_const(event);
  const bt_field* packetContext = bt_packet_borrow_context_field_const(packet);
  AddFieldStruct(jobj, "packet_context", packetContext);
}

static void AddStreamEventContext(json_object* jobj, const bt_event* event) {
  const bt_field* streamEventContext = bt_event_borrow_common_context_field_const(event);
  if (streamEventContext) {
    AddFieldStruct(jobj, "stream_event_context", streamEventContext);
  }
}

static void AddEventContext(json_object* jobj, const bt_event* event) {
  const bt_field* eventContext = bt_event_borrow_specific_context_field_const(event);

  if (eventContext) {
    AddFieldStruct(jobj, "event_context", eventContext);
  }
}

static void AddEventHeader(json_object* jobj, const bt_event* event) {
  const bt_packet* packet = bt_event_borrow_packet_const(event);
  const bt_stream* stream = bt_packet_borrow_stream_const(packet);
  const bt_trace* trace = bt_stream_borrow_trace_const(stream);

  json_object* event_hearder_jobj = json_object_new_object();
  json_object_object_add(jobj, "event_header", event_hearder_jobj);

  {
    const char* traceName = bt_trace_get_name(trace);
    if (!traceName) {
      traceName = "Unknown";
    }

    json_object_object_add(event_hearder_jobj, "trace", json_object_new_string(traceName));

    uint64_t count = bt_trace_get_environment_entry_count(trace);
    for (uint64_t i = 0; i < count; i++) {
      const char* name = NULL;
      const bt_value* val = NULL;
      bt_trace_borrow_environment_entry_by_index_const(trace, i, &name, &val);
      // TODO: Add these values to the jsonBuilder
    }
  }
}
