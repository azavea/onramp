# Deployment

This document walks through instantiating an EC2 instance that will contain a
copy of this repository, as well as a background daemon will update the OSMX
database using the minutely diffs of `planet.osm` [provided by
OpenStreetMap](https://planet.openstreetmap.org/replication/minute/).

An AWS account with an existing EBS snapshot containing a `planet.osmx` database
file is required.

## Workflow

The following AWS CLI command will instantiate the EC2 instance. You will have
to substitute your own key name, security group, subnet ID, and snapshot ID.

```bash
aws ec2 run-instances \
    --image-id ami-0bcc094591f354be2 \
    --count 1 \
    --instance-type t3.xlarge \
    --key-name joker \
    --security-group-ids sg-joker \
    --subnet-id subnet-joker \
    --block-device-mappings "[{\"DeviceName\":\"/dev/sdf\",\"Ebs\":{\"SnapshotId\":\"snap-joker\"}}]" \
    --user-data file://cloud-config.yml
```

After the EC2 instance is online, you can follow the progress of `osmx-update`
with the following command:

```bash
$ tail -f /var/log/syslog
Aug 31 23:48:56 ip-10-0-1-70 osmx-update: Committed: 3985436 -> 3985437 in 17.995 seconds.
Aug 31 23:49:18 ip-10-0-1-70 osmx-update: Committed: 3985437 -> 3985438 in 21.765 seconds.
Aug 31 23:49:48 ip-10-0-1-70 osmx-update: Committed: 3985438 -> 3985439 in 29.088 seconds.
. . .
```
