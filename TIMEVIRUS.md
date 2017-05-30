The badges contain a time virus. If you set the current time with:

In python, get the current time_t value for localtime as an integer.

>>> import datetime
>>> print int(time.mktime(datetime.datetime.now().timetuple()))

On a badge with BLACK_BADGE set, type:

hail> clock set rtc nnnnnn

Where nnnnn is the value of the time_t.

This will replicate to every badge in radio range and keep going.

If time is available, badges will also display the current temperature in farenheit.



