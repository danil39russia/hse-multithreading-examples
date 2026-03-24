<img width="779" height="574" alt="image" src="https://github.com/user-attachments/assets/05dfb2ca-c0f4-46b8-a30c-c541820bd351" />

Реализован буферизованный канал (FIFO, ёмкость n): Send кладёт в конец очереди и блокируется при полном буфере; Recv забирает с головы и блокируется при пустой очереди; Close запрещает дальнейшие Send (std::runtime_error) и после опустошения буфера Recv возвращает пустой std::optional. Синхронизация: mutex + два condition_variable. Тесты (gtest) и бенчмарк проходят, время бенчмарка < 8 с.

