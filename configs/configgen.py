import jinja2
import json
from collections import OrderedDict
import sys

#
# About this script: Envoy configurations needed for a complete infrastructure are complicated.
# This script demonstrates how to programatically build Envoy configurations using jinja templates.
# This is roughly how we build our configurations at Lyft. The three configurations demonstrated
# here (front proxy, double proxy, and service to service) are also very close approximations to
# what we use at Lyft in production. They give a demonstration of how to configure most Envoy
# features. Along with the configuration guide it should be possible to modify them for different
# use cases.
#

# This is the set of internal services that front Envoy will route to. Each cluster referenced
# in envoy_router.template.json must be specified here. It is a dictionary of dictionaries.
# Options can be specified for each cluster if needed. See make_route_internal() in
# routing_helper.template.json for the types of options supported.
front_envoy_clusters = {
    'service1': {},
    'service2': {},
    'service3': {},
}

# This is the set of internal services that local Envoys will route to. All services that will be
# accessed via the 9001 egress port need to be listed here. It is a dictionary of dictionaries.
# Options can be specified for each cluster if needed. See make_route_internal() in
# routing_helper.template.json for the types of options supported.
service_to_service_envoy_clusters = {
    'ratelimit': {},
    'service1': {},
    'service3': {}
}

# This is a list of external hosts that can be accessed from local Envoys. Each external service has
# its own port. This is because some SDKs don't make it easy to use host based routing. Below
# we demonstrate setting up proxying for DynamoDB. In the config, this ends up using the HTTP
# DynamoDB statistics filter, as well as generating a special access log which includes the
# X-AMZN-RequestId response header.
external_virtual_hosts = [
{
    'name': 'dynamodb_iad',
    'address': "tcp://127.0.0.1:9204",
    'hosts': [
        {
            'name': 'dynamodb_iad', 'domain': '*',
            'remote_address': 'dynamodb.us-east-1.amazonaws.com:443',
            'verify_subject_alt_name': [ 'dynamodb.us-east-1.amazonaws.com' ],
            'ssl': True
        }
    ],
    'is_amzn_service': True,
    'cluster_type': 'logical_dns'
}]

# This is the set of mongo clusters that local Envoys can talk to. Each database defines a set of
# mongos routers to talk to, and whether the global rate limit service should be called for new
# connections. Many organizations will not be interested in the mongo feature. Setting this to
# an empty dictionary will remove all mongo configuration. The configuration is a useful example
# as it demonstrates how to setup TCP proxy and the network rate limit filter.
mongos_servers = {
    'somedb': {
        'address': "tcp://127.0.0.1:27019",
        'hosts': [
            "router1.yourcompany.net:27817",
            "router2.yourcompany.net:27817",
            "router3.yourcompany.net:27817",
            "router4.yourcompany.net:27817",
        ],
        'ratelimit': True
    }
}

def generate_config(template_path, template, output_file, **context):
    """ Generate a final config file based on a template and some context. """
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_path),
                             undefined=jinja2.StrictUndefined)
    raw_output = env.get_template(template).render(**context)
    # Verify valid JSON and then dump it nicely formatted to avoid jinja pain.
    output = json.loads(raw_output, object_pairs_hook=OrderedDict)
    with open(output_file, 'w') as fh:
        json.dump(output, fh, indent=2)

# Generate a demo config for the main front proxy. This sets up both HTTP and HTTPS listeners,
# as well as a listener for the double proxy to connect to via SSL client authentication.
generate_config('configs', 'envoy_front_proxy.template.json',
                '{}/envoy_front_proxy.json'.format(sys.argv[1]), clusters=front_envoy_clusters)

# Generate a demo config for the double proxy. This sets up both an HTTP and HTTPS listeners,
# and backhauls the traffic to the main front proxy.
generate_config('configs', 'envoy_double_proxy.template.json',
                '{}/envoy_double_proxy.json'.format(sys.argv[1]))

# Generate a demo config for the service to service (local) proxy. This sets up several different
# listeners:
# 9211: Main ingress listener for service to service traffic.
# 9001: Main egress listener for service to service traffic. Applications use this port to send
#       requests to other services.
# optional external service ports: built from external_virtual_hosts above. Each external host
#                                  that Envoy proxies to listens on its own port.
# optional mongo ports: built from mongos_servers above.
generate_config('configs', 'envoy_service_to_service.template.json',
                '{}/envoy_service_to_service.json'.format(sys.argv[1]),
                internal_virtual_hosts=service_to_service_envoy_clusters,
                external_virtual_hosts=external_virtual_hosts,
                mongos_servers=mongos_servers)
