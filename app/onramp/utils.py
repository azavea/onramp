from io import BytesIO
from math import floor
import os
from urllib.parse import urlparse

import boto3


def datetime_to_adiff_sequence(datetime):
    """ Convert a timezone aware datetime to minutely augmented diff sequence id. """
    if datetime.tzinfo is None:
        raise ValueError("Datetime {} must be timezone aware".format(datetime))
    query = datetime.timestamp()
    return int(floor((int(query) - 1347432960) / 60))


# pretty print helper
# http://effbot.org/zone/element-lib.htm#prettyprint
def indent(elem, level=0):
    i = "\n" + level * "  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent(elem, level + 1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


def write_augmented_diff_status(output_path, sequence_id):
    """ Write status.txt containing the sequence_id to output_path/state.txt

    Supports s3 and local filesystem paths.

    """
    status_filename = "status.txt"
    if output_path.startswith("s3"):
        url = urlparse(output_path)
        s3 = boto3.resource("s3")
        bucket = s3.Bucket(url.netloc)
        with BytesIO() as fp:
            fp.write(str(sequence_id).encode("utf-8"))
            fp.seek(0)
            key_path = os.path.join(url.path.strip("/"), status_filename)
            bucket.upload_fileobj(fp, key_path, {"ContentType": "text/plain"})
    else:
        with open(os.path.join(output_path, status_filename), "w") as fp:
            fp.write(str(sequence_id))
