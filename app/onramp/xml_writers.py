import gzip
from io import BytesIO
import logging
import os
import shutil
import sys
from urllib.parse import urlparse

import boto3

writer_logger = logging.getLogger(__name__)
writer_logger.setLevel(logging.INFO)
writer_logger.addHandler(logging.StreamHandler(sys.stdout))


def write_xml(element_tree, output_file, logger=None):
    """ Write xml to output_file

    Autoselects appropriate writer based on output_file:
    Supports s3 and local file uris, will automatically gzip
    XML if output_file ends in .gz

    """
    if output_file.startswith("s3"):
        if output_file.endswith(".gz"):
            s3_gzip_writer(element_tree, output_file, logger=logger)
        else:
            s3_writer(element_tree, output_file, logger=logger)
    elif output_file.endswith(".gz"):
        file_gzip_writer(element_tree, output_file, logger=logger)
    else:
        file_writer(element_tree, output_file, logger=logger)


def s3_gzip_writer(element_tree, output_file, logger=None):
    """ Write element_tree as gzipped xml to S3

    element_tree: xml.etree.ElementTree.ElementTree
    output_file: S3 URI

    """
    if logger is None:
        logger = writer_logger
    url = urlparse(output_file)
    s3 = boto3.resource("s3")
    bucket = s3.Bucket(url.netloc)
    key_path = url.path.lstrip("/")
    with BytesIO() as compressed_fp:
        # Write to Gzip file handle in order to compress directly to the
        # compressed_fp in memory buffer
        with gzip.GzipFile(fileobj=compressed_fp, mode="wb") as gz:
            element_tree.write(gz)
        compressed_fp.seek(0)
        bucket.upload_fileobj(
            compressed_fp,
            key_path,
            {"ContentType": "text/xml", "ContentEncoding": "gzip"},
        )
    logger.info(
        "Augmented Diff written to: Bucket {}, Path: {} (gzip=True)".format(
            url.netloc, key_path
        )
    )


def s3_writer(element_tree, output_file, logger=None):
    """ Write element_tree as xml to S3

    element_tree: xml.etree.ElementTree.ElementTree
    output_file: S3 URI

    """
    if logger is None:
        logger = writer_logger
    url = urlparse(output_file)
    s3 = boto3.resource("s3")
    bucket = s3.Bucket(url.netloc)
    with BytesIO() as fp:
        element_tree.write(fp)
        fp.seek(0)
        key_path = url.path.lstrip("/")
        bucket.upload_fileobj(fp, key_path, {"ContentType": "text/xml"})
    logger.info(
        "Augmented Diff written to: Bucket {}, Path: {} (gzip=False)".format(
            url.netloc, key_path
        )
    )


def file_gzip_writer(element_tree, output_file, logger=None):
    """ Write element_tree as gzipped xml to file

    element_tree: xml.etree.ElementTree.ElementTree
    output_file: Local or absolute string filepath

    """
    if logger is None:
        logger = writer_logger
    output_path, _ = os.path.split(output_file)
    if len(output_path) > 0:
        os.makedirs(output_path, exist_ok=True)
    with gzip.open(output_file, "wb") as fp:
        element_tree.write(fp)
    logger.info("Augmented Diff written to: {} (gzip=True)".format(output_file))


def file_writer(element_tree, output_file, logger=None):
    """ Write element_tree as xml to file

    element_tree: xml.etree.ElementTree.ElementTree
    output_file: Local or absolute string filepath

    """
    if logger is None:
        logger = writer_logger
    output_path, _ = os.path.split(output_file)
    if len(output_path) > 0:
        os.makedirs(output_path, exist_ok=True)
    with open(output_file, "wb") as fp:
        element_tree.write(fp)
    logger.info("Augmented Diff written to: {} (gzip=False)".format(output_file))
