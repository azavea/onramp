FROM osmx:latest

RUN apt update \
  && apt install --no-install-recommends -y \
    cython3 \
    gcc \
    g++ \
    make \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
  && rm -rf /var/lib/apt/lists
COPY requirements.txt /requirements.txt
RUN pip3 install -r /requirements.txt

COPY . /app
WORKDIR /app
ENTRYPOINT ["python3", "augmented_diff.py"]
