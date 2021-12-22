#pragma once

#include <assert.h>
#include <babeltrace2/babeltrace.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "decode_event.h"

static void CheckBtError(int32_t status) {
  switch (status) {
    case BT_GRAPH_RUN_STATUS_OK:
      return;
    case BT_GRAPH_RUN_STATUS_MEMORY_ERROR:
      quick_exit(EXIT_FAILURE);
    default:
      quick_exit(EXIT_FAILURE);
  }
}

/*
 * Shared data between our relay sink component and the
 * bt_graph_run_once() call site.
 */
typedef struct relay_data {
  bt_message_array_const msgs;
  uint64_t msg_count;
} relay_data;

/*
 * Consumer method of our relay sink component class.
 *
 * See
 * <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-graph.html#ga301a677396bd8f5bd8b920fd5fa60418>.
 */
static bt_graph_simple_sink_component_consume_func_status relay_consume(
    bt_message_iterator* const msg_iter,
    void* const user_data) {
  struct relay_data* const relay_data = (struct relay_data*)user_data;
  bt_message_iterator_next_status msg_iter_next_status;
  bt_graph_simple_sink_component_consume_func_status status =
      BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK;

  /* Consume the next messages, storing them in `*relay_data` */
  msg_iter_next_status =
      bt_message_iterator_next(msg_iter, &relay_data->msgs, &relay_data->msg_count);
  switch (msg_iter_next_status) {
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:

      status = BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END;
      break;
    case BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR:

      status = BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR;
      break;
    case BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR:

      status = BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR;
      break;
    default:
      assert(status == BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK);
      break;
  }

  return status;
}

/*
 * Adds our own sink component named `relay` to the trace processing
 * graph `graph`.
 *
 * On success, `*comp` is the added sink component.
 *
 * See <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-graph.html#api-graph-lc-add-ss>.
 */
static bt_graph_add_component_status add_relay_comp(bt_graph* const graph,
                                                    struct relay_data* const relay_data,
                                                    const bt_component_sink** const comp) {
  return bt_graph_add_simple_sink_component(graph, "relay", NULL, relay_consume, NULL, relay_data,
                                            comp);
}

/*
 * Creates and returns the parameters to initialize the `src.ctf.lttng-live`
 * component with the trace directory `trace_dir`.
 *
 * See <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-val.html>.
 */
static bt_value* create_ctf_lttng_live_comp_params(const char* const listening_url) {
  bt_value* params;
  bt_value* inputs = NULL;
  bt_value_map_insert_entry_status insert_entry_status;
  bt_value_array_append_element_status append_elem_status;

  bt_value* url_array = bt_value_array_create();

  params = bt_value_map_create();
  if (!params) {
    BT_VALUE_PUT_REF_AND_RESET(params);
    BT_VALUE_PUT_REF_AND_RESET(url_array);
    return params;
  }

  CheckBtError(bt_value_map_insert_entry(params, "inputs", url_array));
  CheckBtError(bt_value_map_insert_string_entry(params, "session-not-found-action", "continue"));
  CheckBtError(bt_value_array_append_string_element(url_array, listening_url));

  return params;
}

/*
 * Adds a `src.ctf.lttng-live` component named `ctf` to read the trace directory
 * `trace_dir` to the trace processing graph `graph`.
 *
 * See <https://babeltrace.org/docs/v2.0/man7/babeltrace2-source.ctf.fs.7/>.
 *
 * On success, `*comp` is the added source component.
 */
static bt_graph_add_component_status add_ctf_lttng_live_comp(
    bt_graph* const graph,
    const char* const listening_url,
    const bt_component_source** const comp) {
  const bt_plugin* plugin;
  bt_plugin_find_status plugin_find_status;
  const bt_component_class_source* comp_cls;
  bt_value* params = NULL;
  bt_graph_add_component_status add_comp_status;

  /* Find the `ctf` plugin */
  plugin_find_status = bt_plugin_find("ctf", BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, &plugin);
  if (plugin_find_status != BT_PLUGIN_FIND_STATUS_OK) {
    puts("Failed to find 'ctf' plugin.");
    goto error;
  }

  /* Borrow the `lttng-live` source component class within the `ctf` plugin */
  comp_cls = bt_plugin_borrow_source_component_class_by_name_const(plugin, "lttng-live");
  if (!comp_cls) {
    puts("Failed to find component 'lttng-live' in 'ctf' plugin.");
    goto error;
  }

  /* Create the parameters to initialize the source component */
  params = create_ctf_lttng_live_comp_params(listening_url);
  if (!params) {
    puts("Failed to create ctf.lttng-live parameters");
    goto error;
  }

  /* Add the source component to the graph */
  add_comp_status =
      bt_graph_add_source_component(graph, comp_cls, "ctf", params, BT_LOGGING_LEVEL_NONE, comp);
  goto end;

error:
  add_comp_status = BT_GRAPH_ADD_COMPONENT_STATUS_ERROR;

end:
  bt_plugin_put_ref(plugin);
  bt_value_put_ref(params);
  return add_comp_status;
}

/*
 * Adds a `flt.utils.muxer` component named `muxer` to the trace
 * processing graph `graph`.
 *
 * See <https://babeltrace.org/docs/v2.0/man7/babeltrace2-filter.utils.muxer.7/>.
 *
 * On success, `*comp` is the added filter component.
 */
static bt_graph_add_component_status add_muxer_comp(bt_graph* const graph,
                                                    const bt_component_filter** const comp) {
  const bt_plugin* plugin;
  bt_plugin_find_status plugin_find_status;
  const bt_component_class_filter* comp_cls;
  bt_graph_add_component_status add_comp_status;

  /* Find the `utils` plugin */
  plugin_find_status =
      bt_plugin_find("utils", BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, &plugin);
  if (plugin_find_status != BT_PLUGIN_FIND_STATUS_OK) {
    goto error;
  }

  /* Borrow the `muxer` filter comp. class within the `utils` plugin */
  comp_cls = bt_plugin_borrow_filter_component_class_by_name_const(plugin, "muxer");
  if (!comp_cls) {
    goto error;
  }

  /* Add the filter component to the graph (no init. parameters) */
  add_comp_status =
      bt_graph_add_filter_component(graph, comp_cls, "muxer", NULL, BT_LOGGING_LEVEL_NONE, comp);
  goto end;

error:
  add_comp_status = BT_GRAPH_ADD_COMPONENT_STATUS_ERROR;

end:
  bt_plugin_put_ref(plugin);
  return add_comp_status;
}

/*
 * Creates a trace processing graph having this layout:
 *
 *     +------------+    +-----------------+    +--------------+
 *     | src.ctf.fs |    | flt.utils.muxer |    | Our own sink |
 *     |    [ctf]   |    |     [muxer]     |    |    [relay]   |
 *     |            |    |                 |    |              |
 *     |    stream0 @--->@ in0         out @--->@ in           |
 *     |    stream1 @--->@ in1             |    +--------------+
 *     |    stream2 @--->@ in2             |
 *     |    stream3 @--->@ in3             |
 *     +------------+    @ in4             |
 *                       +-----------------+
 *
 * In the example above, the `src.ctf.fs` component reads a CTF trace
 * having four data streams. The `trace_dir` parameter is the directory
 * containing the CTF trace to read.
 *
 * Our own relay sink component, of which the consuming method is
 * relay_consume(), consumes messages from the `flt.utils.muxer`
 * component, storing them to a structure (`*relay_data`) shared with
 * the bt_graph_run_once() call site.
 *
 * See <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-graph.html>.
 */
static bt_graph* create_graph(const char* const trace_dir, struct relay_data* const relay_data) {
  bt_graph* graph;
  const bt_component_source* ctf_lttng_live_comp;
  const bt_component_filter* muxer_comp;
  const bt_component_sink* relay_comp;
  bt_graph_add_component_status add_comp_status;
  bt_graph_connect_ports_status connect_ports_status;
  uint64_t i;

  /* Create an empty trace processing graph */
  graph = bt_graph_create(0);
  if (!graph) {
    puts("Failed to create an empty trace processing graph.");
    goto error;
  }

  /* Create and add the three required components to `graph` */
  add_comp_status = add_muxer_comp(graph, &muxer_comp);
  if (add_comp_status != BT_GRAPH_ADD_COMPONENT_STATUS_OK) {
    puts("Failed to add component 'muxer'.");
    goto error;
  }

  add_comp_status = add_relay_comp(graph, relay_data, &relay_comp);
  if (add_comp_status != BT_GRAPH_ADD_COMPONENT_STATUS_OK) {
    puts("Failed to add component 'relay'.");
    goto error;
  }

  add_comp_status = add_ctf_lttng_live_comp(graph, trace_dir, &ctf_lttng_live_comp);
  if (add_comp_status != BT_GRAPH_ADD_COMPONENT_STATUS_OK) {
    printf("Failed to add component 'ctf_lttng_live'. Returned status: %d\n", add_comp_status);
    goto error;
  }

  /*
   * Connect all the output ports of the `ctf` source component to
   * the input ports of the `muxer` filter component.
   *
   * An `flt.utils.muxer` component adds an input port every time
   * you connect one, making one always available.
   *
   * See <https://babeltrace.org/docs/v2.0/man7/babeltrace2-filter.utils.muxer.7/#doc-_input>.
   */
  for (i = 0; i < bt_component_source_get_output_port_count(ctf_lttng_live_comp); i++) {
    const bt_port_output* const out_port =
        bt_component_source_borrow_output_port_by_index_const(ctf_lttng_live_comp, i);
    const bt_port_input* const in_port =
        bt_component_filter_borrow_input_port_by_index_const(muxer_comp, i);

    /* Connect ports */
    connect_ports_status = bt_graph_connect_ports(graph, out_port, in_port, NULL);

    if (connect_ports_status != BT_GRAPH_CONNECT_PORTS_STATUS_OK) {
      goto error;
    }
  }

  /* Connect the `muxer` output port to the `relay` input port */
  connect_ports_status = bt_graph_connect_ports(
      graph, bt_component_filter_borrow_output_port_by_index_const(muxer_comp, 0),
      bt_component_sink_borrow_input_port_by_index_const(relay_comp, 0), NULL);
  if (connect_ports_status != BT_GRAPH_CONNECT_PORTS_STATUS_OK) {
    goto error;
  }

  goto end;

error:
  BT_GRAPH_PUT_REF_AND_RESET(graph);

end:
  return graph;
}

/*
 * Handles a single message `msg`, printing its name if it's an event
 * message.
 *
 * See <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-msg.html>.
 */
static char const* handle_msg(const bt_message* const msg) {
  if (bt_message_get_type(msg) != BT_MESSAGE_TYPE_EVENT) {
    return NULL;
  }

  const bt_event* event = bt_message_event_borrow_event_const(msg);
  const bt_event_class* eventClass = bt_event_borrow_class_const(event);
  json_object* const jobj = json_object_new_object();

  AddEventName(jobj, eventClass);

  const bt_clock_snapshot* clock = bt_message_event_borrow_default_clock_snapshot_const(msg);
  AddTimestamp(jobj, clock);

  AddPacketContext(jobj, event);
  AddEventHeader(jobj, event);
  AddStreamEventContext(jobj, event);
  AddEventContext(jobj, event);
  AddPayload(jobj, event);
  char const* output_message = json_object_to_json_string_ext(
      jobj, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
  return output_message;
}

/*
 * Runs the trace processing graph `graph`, our relay sink component
 * transferring its consumed messages to `*relay_data`.
 *
 * See <https://babeltrace.org/docs/v2.0/libbabeltrace2/group__api-graph.html#api-graph-lc-run>.
 */
static const char** run_graph_once(bt_graph* const graph, struct relay_data* const relay_data) {
  /*
   * bt_graph_run_once() calls the consuming method of
   * our relay sink component (relay_consume()).
   *
   * relay_consume() consumes a batch of messages from the
   * `flt.utils.muxer` component and stores them in
   * `*relay_data`.
   */
  bt_graph_run_once_status status = bt_graph_run_once(graph);
  assert(status != BT_GRAPH_RUN_ONCE_STATUS_AGAIN);
  if (status != BT_GRAPH_RUN_ONCE_STATUS_OK) {
    return NULL;
  }

  const char** output_messages = (const char**)malloc((relay_data->msg_count) * sizeof(char*));

  /* Handle each consumed message */
  for (uint64_t i = 0; i < relay_data->msg_count; i++) {
    const bt_message* const msg = relay_data->msgs[i];

    output_messages[i] = handle_msg(msg);

    /*
     * The message reference `msg` is ours: release
     * it now.
     */
    bt_message_put_ref(msg);
  }

  return output_messages;
}
