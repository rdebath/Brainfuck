#!/bin/sh

bf testing/Mandelbrot-extreme.b |
{
    echo 'P5'
    echo '1024 768'
    echo '255'
    sed 's/......//' | tr ' ' '@' | tr -d '\012'
} | ppmnorm | pnmtopng > mandelbrot-extreme.png
