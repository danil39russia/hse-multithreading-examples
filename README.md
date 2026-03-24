<img width="394" height="116" alt="image" src="https://github.com/user-attachments/assets/b91873aa-dda4-414f-801c-fdac56211c0a" />

Реализован ProcessPool (15 баллов): задачи выполняются в пуле процессов (fork + pipe), а Submit возвращает свой MyFuture.
Сделана передача результатов и ошибок из дочерних процессов в родительский; future.Get() корректно возвращает значение или кидает исключение.
Демо в tasks/future/process_pool_demo.cpp собирается и успешно запускается.
