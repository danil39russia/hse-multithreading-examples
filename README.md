<img width="539" height="179" alt="image" src="https://github.com/user-attachments/assets/288db416-0d4c-427f-a8ea-59fbd1333867" />
Сделан FutexMutex (аналог std::mutex) в tasks/futex: lock/try_lock/unlock через futex на Linux.
В test.cpp два простых сценария (стресс-счётчик и проверка try_lock) + вывод в консоль All futex mutex checks passed.

вывод в консоль просто потому что люблю визуализация, а то код 0 - пустоватенький
