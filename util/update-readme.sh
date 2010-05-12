#!/bin/bash

perl util/wiki2pod.pl doc/manpage.wiki > /tmp/a.pod \
    && pod2text /tmp/a.pod > README \
    && perl -i -pe 's{(https?://.*?)>}{$1 >}g' README

